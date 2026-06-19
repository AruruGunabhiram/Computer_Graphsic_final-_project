/*
 * CSCI 4229/5229 Final Project
 * Gunabhiram Aruru
 *
 * Shader-Based Renewable Energy Farm Visualization
 * Final review build: textured renewable-energy farm with coupled turbine
 * and GLSL wind animation, transparency, lighting, and distance fog.
 */

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <GLUT/glut.h>
#else
#ifdef USEGLEW
#include <GL/glew.h>
#else
#define GL_GLEXT_PROTOTYPES
#endif
#include <GL/glut.h>
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#define Cos(x) std::cos((x) * 3.1415927 / 180.0)
#define Sin(x) std::sin((x) * 3.1415927 / 180.0)

struct Instance
{
   double x;
   double y;
   double z;
   double sx;
   double sy;
   double sz;
   double rotation;
   float r;
   float g;
   float b;
   double bladeAngle;
};

struct FenceSection
{
   double x;
   double z;
   double length;
   double rotation;
};

// -----------------------------------------------------------------------------
// Constants and global application state
// -----------------------------------------------------------------------------

const double minWindSpeed = 0.0;
const double maxWindSpeed = 5.0;
const double windSpeedStep = 0.25;
const double baseBladeDegreesPerSecond = 45.0;
const float fogColor[] = {0.12f, 0.16f, 0.20f, 1.0f};
const float fogStart = 14.0f;
const float fogEnd = 32.0f;

int axes = 1;
int mode = 1;
int inspectionMode = 0;
int rotateBlades = 1;
int lighting = 1;
int textures = 1;
int glassVisible = 1;
int moveLight = 1;
int shaderEnabled = 1;
int windFlowVisible = 1;
int fogEnabled = 1;
GLuint windProgram = 0;
double windSpeed = 1.0;
double lightAngle = 90;
double lightHeight = 5;
int th = 35;
int ph = 25;
int fov = 60;
double asp = 1;
double dim = 9;
double bladeAngle = 0;
double fpX = 0;
double fpY = 1;
double fpZ = 12;
int fpYaw = 0;
double viewTargetX = 0;
double viewTargetY = 1;
double viewTargetZ = 0;
int windowWidth = 800;
int windowHeight = 600;
unsigned int textureGrass = 0;
unsigned int textureWood = 0;
unsigned int textureRoof = 0;
unsigned int texturePath = 0;
unsigned int textureMetal = 0;

// -----------------------------------------------------------------------------
// General utilities
// -----------------------------------------------------------------------------

// Reverse byte order when reading a BMP created on opposite-endian hardware.
void Reverse(void* value, int bytes)
{
   char* data = static_cast<char*>(value);
   for (int i = 0; i < bytes / 2; ++i)
   {
      const char temp = data[i];
      data[i] = data[bytes - 1 - i];
      data[bytes - 1 - i] = temp;
   }
}

// Report an unrecoverable texture-loading error and stop the program.
void TextureError(const char* message, const char* file)
{
   std::fprintf(stderr, "Texture error: %s: %s\n", message, file);
   std::exit(1);
}

// Report the current OpenGL error, if any, with location context.
static void ErrCheck(const char* where)
{
   const int err = glGetError();
   if (err)
      std::fprintf(stderr, "ERROR: %s [%s]\n", gluErrorString(err), where);
}

// Course-style loader for uncompressed 24-bit BMP texture files.
unsigned int LoadTexBMP(const char* file)
{
   FILE* input = std::fopen(file, "rb");
   if (!input)
      TextureError("cannot open file", file);

   unsigned short magic;
   if (std::fread(&magic, 2, 1, input) != 1)
      TextureError("cannot read BMP magic", file);
   if (magic != 0x4D42 && magic != 0x424D)
      TextureError("file is not a BMP", file);

   unsigned int offset;
   unsigned int width;
   int height;
   unsigned short planes;
   unsigned short bitsPerPixel;
   unsigned int compression;
   if (std::fseek(input, 8, SEEK_CUR) ||
       std::fread(&offset, 4, 1, input) != 1 ||
       std::fseek(input, 4, SEEK_CUR) ||
       std::fread(&width, 4, 1, input) != 1 ||
       std::fread(&height, 4, 1, input) != 1 ||
       std::fread(&planes, 2, 1, input) != 1 ||
       std::fread(&bitsPerPixel, 2, 1, input) != 1 ||
       std::fread(&compression, 4, 1, input) != 1)
      TextureError("cannot read BMP header", file);

   if (magic == 0x424D)
   {
      Reverse(&offset, 4);
      Reverse(&width, 4);
      Reverse(&height, 4);
      Reverse(&planes, 2);
      Reverse(&bitsPerPixel, 2);
      Reverse(&compression, 4);
   }

   const unsigned int imageHeight = height < 0 ? -height : height;
   int maxTextureSize;
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
   if (width < 1 || width > static_cast<unsigned int>(maxTextureSize) ||
       imageHeight < 1 || imageHeight > static_cast<unsigned int>(maxTextureSize))
      TextureError("texture dimensions are out of range", file);
   if ((width & (width - 1)) || (imageHeight & (imageHeight - 1)))
      TextureError("texture dimensions must be powers of two", file);
   if (planes != 1 || bitsPerPixel != 24 || compression != 0)
      TextureError("BMP must be uncompressed 24-bit RGB", file);

   const unsigned int size = 3 * width * imageHeight;
   unsigned char* image = static_cast<unsigned char*>(std::malloc(size));
   if (!image)
      TextureError("cannot allocate image memory", file);
   if (std::fseek(input, offset, SEEK_SET) || std::fread(image, size, 1, input) != 1)
      TextureError("cannot read BMP pixels", file);
   std::fclose(input);

   for (unsigned int i = 0; i < size; i += 3)
   {
      const unsigned char temp = image[i];
      image[i] = image[i + 2];
      image[i + 2] = temp;
   }

   unsigned int texture;
   glGenTextures(1, &texture);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, imageHeight, 0,
                GL_RGB, GL_UNSIGNED_BYTE, image);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   std::free(image);
   return texture;
}

// -----------------------------------------------------------------------------
// Camera, projection, and HUD
// -----------------------------------------------------------------------------

// Return the readable name of the active camera projection.
const char* ModeName()
{
   switch (mode)
   {
      case 0: return "Oblique overhead orthogonal";
      case 1: return "Oblique overhead perspective";
      default: return "First person perspective";
   }
}

// Return a readable label for the currently selected object group.
const char* InspectionName()
{
   switch (inspectionMode)
   {
      case 0: return "Full scene";
      case 1: return "Turbines";
      case 2: return "Barn/farmhouse";
      case 3: return "Greenhouse";
      case 4: return "Solar panels";
      default: return "Fence and secondary objects";
   }
}

// Draw bitmap text in the active orthographic HUD projection.
void DrawText(int x, int y, const char* text)
{
   glRasterPos2i(x, y);
   for (const char* ch = text; *ch; ++ch)
      glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *ch);
}

// Configure orthographic or perspective projection for the active camera mode.
void Project()
{
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   // Mode 0 uses an orthogonal volume; modes 1 and 2 share perspective.
   if (mode == 0)
   {
      glOrtho(-asp * dim, asp * dim, -dim, dim, -4 * dim, 4 * dim);
   }
   else
   {
      gluPerspective(fov, asp, 0.1, 100);
   }

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
}

// Draw optional world-coordinate axes for navigation and grading.
void DrawAxes()
{
   const double len = 2.0;

   glColor3f(1, 1, 1);
   glBegin(GL_LINES);
   glVertex3d(0, 0, 0);
   glVertex3d(len, 0, 0);
   glVertex3d(0, 0, 0);
   glVertex3d(0, len, 0);
   glVertex3d(0, 0, 0);
   glVertex3d(0, 0, len);
   glEnd();

   glRasterPos3d(len, 0, 0);
   glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'X');
   glRasterPos3d(0, len, 0);
   glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'Y');
   glRasterPos3d(0, 0, len);
   glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'Z');
}

// -----------------------------------------------------------------------------
// Primitive geometry
// -----------------------------------------------------------------------------

// Draw a unit textured box with outward normals on all six faces.
void drawBoxUnit(double repeatX = 1, double repeatY = 1, double repeatZ = 1)
{
   glBegin(GL_QUADS);

   glNormal3f(0, 0, 1);
   glTexCoord2f(0, 0); glVertex3d(-0.5, -0.5,  0.5);
   glTexCoord2d(repeatX, 0); glVertex3d( 0.5, -0.5,  0.5);
   glTexCoord2d(repeatX, repeatY); glVertex3d( 0.5,  0.5,  0.5);
   glTexCoord2d(0, repeatY); glVertex3d(-0.5,  0.5,  0.5);

   glNormal3f(0, 0, -1);
   glTexCoord2f(0, 0); glVertex3d( 0.5, -0.5, -0.5);
   glTexCoord2d(repeatX, 0); glVertex3d(-0.5, -0.5, -0.5);
   glTexCoord2d(repeatX, repeatY); glVertex3d(-0.5,  0.5, -0.5);
   glTexCoord2d(0, repeatY); glVertex3d( 0.5,  0.5, -0.5);

   glNormal3f(-1, 0, 0);
   glTexCoord2f(0, 0); glVertex3d(-0.5, -0.5, -0.5);
   glTexCoord2d(repeatZ, 0); glVertex3d(-0.5, -0.5,  0.5);
   glTexCoord2d(repeatZ, repeatY); glVertex3d(-0.5,  0.5,  0.5);
   glTexCoord2d(0, repeatY); glVertex3d(-0.5,  0.5, -0.5);

   glNormal3f(1, 0, 0);
   glTexCoord2f(0, 0); glVertex3d(0.5, -0.5,  0.5);
   glTexCoord2d(repeatZ, 0); glVertex3d(0.5, -0.5, -0.5);
   glTexCoord2d(repeatZ, repeatY); glVertex3d(0.5,  0.5, -0.5);
   glTexCoord2d(0, repeatY); glVertex3d(0.5,  0.5,  0.5);

   glNormal3f(0, 1, 0);
   glTexCoord2f(0, 0); glVertex3d(-0.5, 0.5,  0.5);
   glTexCoord2d(repeatX, 0); glVertex3d( 0.5, 0.5,  0.5);
   glTexCoord2d(repeatX, repeatZ); glVertex3d( 0.5, 0.5, -0.5);
   glTexCoord2d(0, repeatZ); glVertex3d(-0.5, 0.5, -0.5);

   glNormal3f(0, -1, 0);
   glTexCoord2f(0, 0); glVertex3d(-0.5, -0.5, -0.5);
   glTexCoord2d(repeatX, 0); glVertex3d( 0.5, -0.5, -0.5);
   glTexCoord2d(repeatX, repeatZ); glVertex3d( 0.5, -0.5,  0.5);
   glTexCoord2d(0, repeatZ); glVertex3d(-0.5, -0.5,  0.5);
   glEnd();
}

// Place a box primitive with the requested center and dimensions.
void drawBox(double x, double y, double z,
             double sx, double sy, double sz)
{
   glPushMatrix();
   glTranslated(x, y, z);
   glScaled(sx, sy, sz);
   drawBoxUnit();
   glPopMatrix();
}

// Draw one handmade turbine blade with front, back, and edge normals.
void drawBladeUnit()
{
   const double x[5] = {-0.08, 0.17, 0.30, 0.09, -0.16};
   const double y[5] = { 0.20, 0.43, 1.55, 1.78,  0.62};
   const double front = 0.07;
   const double back = -0.07;

   glBegin(GL_POLYGON);
   glNormal3f(0, 0, 1);
   for (int i = 0; i < 5; ++i)
   {
      glTexCoord2d(x[i] + 0.16, y[i] / 1.78);
      glVertex3d(x[i], y[i], front);
   }
   glEnd();

   glBegin(GL_POLYGON);
   glNormal3f(0, 0, -1);
   for (int i = 4; i >= 0; --i)
   {
      glTexCoord2d(x[i] + 0.16, y[i] / 1.78);
      glVertex3d(x[i], y[i], back);
   }
   glEnd();

   glBegin(GL_QUADS);
   for (int i = 0; i < 5; ++i)
   {
      const int next = (i + 1) % 5;
      const double dx = x[next] - x[i];
      const double dy = y[next] - y[i];
      const double length = std::sqrt(dx * dx + dy * dy);
      glNormal3d(dy / length, -dx / length, 0);
      glTexCoord2f(0, 0); glVertex3d(x[i], y[i], front);
      glTexCoord2f(1, 0); glVertex3d(x[next], y[next], front);
      glTexCoord2f(1, 1); glVertex3d(x[next], y[next], back);
      glTexCoord2f(0, 1); glVertex3d(x[i], y[i], back);
   }
   glEnd();
}

// Draw the handmade cylindrical turbine hub.
void drawHubUnit()
{
   const int sides = 12;
   const double radius = 0.22;
   const double front = 0.15;
   const double back = -0.15;

   glBegin(GL_QUAD_STRIP);
   for (int i = 0; i <= sides; ++i)
   {
      const double angle = 360.0 * i / sides;
      glNormal3d(Cos(angle), Sin(angle), 0);
      glTexCoord2d(static_cast<double>(i) / sides, 1);
      glVertex3d(radius * Cos(angle), radius * Sin(angle), front);
      glTexCoord2d(static_cast<double>(i) / sides, 0);
      glVertex3d(radius * Cos(angle), radius * Sin(angle), back);
   }
   glEnd();

   glBegin(GL_POLYGON);
   glNormal3f(0, 0, 1);
   for (int i = 0; i < sides; ++i)
   {
      const double angle = 360.0 * i / sides;
      glTexCoord2d(0.5 + 0.5 * Cos(angle), 0.5 + 0.5 * Sin(angle));
      glVertex3d(radius * Cos(angle), radius * Sin(angle), front);
   }
   glEnd();

   glBegin(GL_POLYGON);
   glNormal3f(0, 0, -1);
   for (int i = sides - 1; i >= 0; --i)
   {
      const double angle = 360.0 * i / sides;
      glTexCoord2d(0.5 + 0.5 * Cos(angle), 0.5 + 0.5 * Sin(angle));
      glVertex3d(radius * Cos(angle), radius * Sin(angle), back);
   }
   glEnd();
}

// Build the turbine support from a square footing, four tapered tower faces,
// and a box nacelle. The face normals account for the taper so lighting remains
// correct, while the rotor is assembled separately from four blades and a hub.
void drawWindmillBaseUnit()
{
   const double taper = 0.21 / 2.61;
   const double normalLength = std::sqrt(1.0 + taper * taper);
   const double normalSide = 1.0 / normalLength;
   const double normalUp = taper / normalLength;

   glPushMatrix();
   glTranslated(0, 0.12, 0);
   glScaled(0.9, 0.24, 0.9);
   drawBoxUnit(2, 1, 2);
   glPopMatrix();

   glBegin(GL_QUADS);
   glNormal3d(0, normalUp, normalSide);
   glTexCoord2f(0, 0); glVertex3d(-0.35, 0.24,  0.35);
   glTexCoord2f(1, 0); glVertex3d( 0.35, 0.24,  0.35);
   glTexCoord2f(1, 3); glVertex3d( 0.14, 2.85,  0.14);
   glTexCoord2f(0, 3); glVertex3d(-0.14, 2.85,  0.14);

   glNormal3d(0, normalUp, -normalSide);
   glTexCoord2f(0, 0); glVertex3d( 0.35, 0.24, -0.35);
   glTexCoord2f(1, 0); glVertex3d(-0.35, 0.24, -0.35);
   glTexCoord2f(1, 3); glVertex3d(-0.14, 2.85, -0.14);
   glTexCoord2f(0, 3); glVertex3d( 0.14, 2.85, -0.14);

   glNormal3d(-normalSide, normalUp, 0);
   glTexCoord2f(0, 0); glVertex3d(-0.35, 0.24, -0.35);
   glTexCoord2f(1, 0); glVertex3d(-0.35, 0.24,  0.35);
   glTexCoord2f(1, 3); glVertex3d(-0.14, 2.85,  0.14);
   glTexCoord2f(0, 3); glVertex3d(-0.14, 2.85, -0.14);

   glNormal3d(normalSide, normalUp, 0);
   glTexCoord2f(0, 0); glVertex3d(0.35, 0.24,  0.35);
   glTexCoord2f(1, 0); glVertex3d(0.35, 0.24, -0.35);
   glTexCoord2f(1, 3); glVertex3d(0.14, 2.85, -0.14);
   glTexCoord2f(0, 3); glVertex3d(0.14, 2.85,  0.14);
   glEnd();

   glPushMatrix();
   glTranslated(0, 2.95, 0.08);
   glScaled(0.65, 0.45, 0.85);
   drawBoxUnit(2, 1, 2);
   glPopMatrix();
}

// -----------------------------------------------------------------------------
// Object geometry
// -----------------------------------------------------------------------------

// Draw a complete turbine model at the origin for later instancing.
void drawWindmillUnit(double bladeOffset)
{
   const float turbineSpecular[] = {0.55f, 0.58f, 0.62f, 1.0f};
   const float defaultSpecular[] = {0.22f, 0.22f, 0.22f, 1.0f};

   // Turbine metal receives a restrained highlight without looking chrome-like.
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, turbineSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 42.0f);

   // The generic windmill stays at the origin so one model can be instanced.
   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, textureMetal);
      glColor3f(0.86f, 0.88f, 0.90f);
   }
   drawWindmillBaseUnit();

   glPushMatrix();
   glTranslated(0, 3.0, 0.58);
   glRotated(bladeAngle + bladeOffset, 0, 0, 1);
   if (textures)
      glColor3f(0.95f, 0.90f, 0.78f);
   else
      glColor3f(0.92f, 0.84f, 0.58f);
   if (textures)
      glBindTexture(GL_TEXTURE_2D, textureWood);
   for (int blade = 0; blade < 4; ++blade)
   {
      glPushMatrix();
      glRotated(90 * blade, 0, 0, 1);
      drawBladeUnit();
      glPopMatrix();
   }
   if (textures)
      glColor3f(0.85f, 0.85f, 0.82f);
   else
      glColor3f(0.32f, 0.25f, 0.18f);
   if (textures)
      glBindTexture(GL_TEXTURE_2D, textureMetal);
   drawHubUnit();
   glPopMatrix();
   glDisable(GL_TEXTURE_2D);

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);
}

// Transform and draw one turbine instance in the farm.
void drawWindmillInstance(const Instance& instance)
{
   // Place, orient, and size the origin-centered model for this farm location.
   glPushMatrix();
   glTranslated(instance.x, instance.y, instance.z);
   glRotated(instance.rotation, 0, 1, 0);
   glScaled(instance.sx, instance.sy, instance.sz);
   glColor3f(instance.r, instance.g, instance.b);
   drawWindmillUnit(instance.bladeAngle);
   glPopMatrix();
}

// Draw the solid ground base, grass texture, and subtle guide grid.
void drawGround()
{
   glPushMatrix();
   glTranslated(0, -0.12, 0);
   glScaled(18, 0.2, 14);
   glColor3f(0.24f, 0.46f, 0.20f);
   drawBoxUnit();
   glPopMatrix();

   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, textureGrass);
      glColor3f(1, 1, 1);
      glBegin(GL_QUADS);
      glNormal3f(0, 1, 0);
      glTexCoord2f(0, 0); glVertex3d(-9, -0.019, -7);
      glTexCoord2f(9, 0); glVertex3d( 9, -0.019, -7);
      glTexCoord2f(9, 7); glVertex3d( 9, -0.019,  7);
      glTexCoord2f(0, 7); glVertex3d(-9, -0.019,  7);
      glEnd();
      glDisable(GL_TEXTURE_2D);
   }

   glColor3f(0.34f, 0.55f, 0.25f);
   glBegin(GL_LINES);
   glNormal3f(0, 1, 0);
   for (int i = -8; i <= 8; ++i)
   {
      glVertex3d(i, 0, -7);
      glVertex3d(i, 0, 7);
   }
   for (int i = -7; i <= 7; ++i)
   {
      glVertex3d(-9, 0, i);
      glVertex3d(9, 0, i);
   }
   glEnd();
}

// Draw the textured path through the farm.
void drawPath()
{
   const double sections[][8] =
   {
      {-1.4,  7.0,  1.4,  7.0,  1.1,  3.5, -1.1,  3.5},
      {-1.1,  3.5,  1.1,  3.5,  2.8,  1.2,  0.8,  1.2},
      { 0.8,  1.2,  2.8,  1.2,  5.2,  2.8,  3.3,  3.5}
   };

   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, texturePath);
      glColor3f(1, 1, 1);
   }
   else
      glColor3f(0.58f, 0.48f, 0.32f);

   glBegin(GL_QUADS);
   glNormal3f(0, 1, 0);
   for (int i = 0; i < 3; ++i)
   {
      glTexCoord2f(0, 0); glVertex3d(sections[i][0], 0.015, sections[i][1]);
      glTexCoord2f(1, 0); glVertex3d(sections[i][2], 0.015, sections[i][3]);
      glTexCoord2f(1, 2); glVertex3d(sections[i][4], 0.015, sections[i][5]);
      glTexCoord2f(0, 2); glVertex3d(sections[i][6], 0.015, sections[i][7]);
   }
   glEnd();
   glDisable(GL_TEXTURE_2D);
}

// Draw one reusable fence section from handmade box geometry.
void drawFenceSection(double x, double z, double length, double rotation)
{
   glPushMatrix();
   glTranslated(x, 0, z);
   glRotated(rotation, 0, 1, 0);
   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, textureWood);
      glColor3f(0.92f, 0.80f, 0.62f);
   }
   else
      glColor3f(0.55f, 0.34f, 0.16f);

   for (double post = -length / 2; post <= length / 2 + 0.01; post += 2.0)
   {
      glPushMatrix();
      glTranslated(post, 0.55, 0);
      glScaled(0.14, 1.1, 0.14);
      drawBoxUnit(1, 2, 1);
      glPopMatrix();
   }

   for (int rail = 0; rail < 2; ++rail)
   {
      glPushMatrix();
      glTranslated(0, 0.38 + 0.42 * rail, 0);
      glScaled(length, 0.12, 0.10);
      drawBoxUnit(length, 1, 1);
      glPopMatrix();
   }
   glDisable(GL_TEXTURE_2D);
   glPopMatrix();
}

// Place fence sections around the scene boundary.
void drawFence()
{
   const FenceSection sections[] =
   {
      { 0, -6.2, 16,  0},
      {-8,  0.0, 12, 90},
      { 8,  0.0, 12, 90},
      {-4,  6.2,  8,  0}
   };

   const int count = sizeof(sections) / sizeof(sections[0]);
   for (int i = 0; i < count; ++i)
   {
      drawFenceSection(sections[i].x, sections[i].z,
                       sections[i].length, sections[i].rotation);
   }
}

// Draw a handmade gable roof with explicitly calculated slope normals.
void drawGableRoofUnit()
{
   const double run = 0.5;
   const double rise = 0.6;
   const double slopeLength = std::sqrt(run * run + rise * rise);
   const double normalY = run / slopeLength;
   const double normalZ = rise / slopeLength;

   glBegin(GL_QUADS);
   glNormal3d(0, normalY, -normalZ);
   glTexCoord2f(0, 0); glVertex3d(-0.5, 0.0, -0.5);
   glTexCoord2f(2, 0); glVertex3d( 0.5, 0.0, -0.5);
   glTexCoord2f(2, 1); glVertex3d( 0.5, 0.6,  0.0);
   glTexCoord2f(0, 1); glVertex3d(-0.5, 0.6,  0.0);

   glNormal3d(0, normalY, normalZ);
   glTexCoord2f(0, 1); glVertex3d(-0.5, 0.6, 0.0);
   glTexCoord2f(2, 1); glVertex3d( 0.5, 0.6, 0.0);
   glTexCoord2f(2, 0); glVertex3d( 0.5, 0.0, 0.5);
   glTexCoord2f(0, 0); glVertex3d(-0.5, 0.0, 0.5);

   glNormal3f(0, -1, 0);
   glTexCoord2f(0, 0); glVertex3d(-0.5, 0.0,  0.5);
   glTexCoord2f(1, 0); glVertex3d( 0.5, 0.0,  0.5);
   glTexCoord2f(1, 1); glVertex3d( 0.5, 0.0, -0.5);
   glTexCoord2f(0, 1); glVertex3d(-0.5, 0.0, -0.5);
   glEnd();

   glBegin(GL_TRIANGLES);
   glNormal3f(-1, 0, 0);
   glTexCoord2f(0, 0);   glVertex3d(-0.5, 0.0, -0.5);
   glTexCoord2f(0.5, 1); glVertex3d(-0.5, 0.6,  0.0);
   glTexCoord2f(1, 0);   glVertex3d(-0.5, 0.0,  0.5);

   glNormal3f(1, 0, 0);
   glTexCoord2f(0, 0);   glVertex3d(0.5, 0.0,  0.5);
   glTexCoord2f(0.5, 1); glVertex3d(0.5, 0.6,  0.0);
   glTexCoord2f(1, 0);   glVertex3d(0.5, 0.0, -0.5);
   glEnd();
}

// Draw the textured barn/shed and its gable roof.
void drawBarnOrShed()
{
   const float barnSpecular[] = {0.08f, 0.07f, 0.06f, 1.0f};
   const float defaultSpecular[] = {0.22f, 0.22f, 0.22f, 1.0f};

   // Wood and painted farm surfaces remain predominantly diffuse.
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, barnSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 8.0f);

   glPushMatrix();
   glTranslated(5.7, 0, 3.4);
   glRotated(-18, 0, 1, 0);

   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, textureWood);
      glColor3f(0.90f, 0.62f, 0.56f);
   }
   else
      glColor3f(0.55f, 0.16f, 0.12f);
   glPushMatrix();
   glTranslated(0, 0.9, 0);
   glScaled(2.4, 1.8, 2.0);
   drawBoxUnit(3, 2, 2);
   glPopMatrix();

   if (textures)
   {
      glBindTexture(GL_TEXTURE_2D, textureRoof);
      glColor3f(0.90f, 0.90f, 0.88f);
   }
   else
      glColor3f(0.30f, 0.12f, 0.09f);
   glPushMatrix();
   glTranslated(0, 1.75, 0);
   glScaled(2.7, 1.15, 2.3);
   drawGableRoofUnit();
   glPopMatrix();

   if (textures)
   {
      glBindTexture(GL_TEXTURE_2D, textureWood);
      glColor3f(0.96f, 0.90f, 0.74f);
   }
   else
      glColor3f(0.80f, 0.72f, 0.52f);
   glPushMatrix();
   glTranslated(-1.21, 0.75, 0);
   glScaled(0.05, 1.15, 0.75);
   drawBoxUnit(1, 2, 1);
   glPopMatrix();
   glDisable(GL_TEXTURE_2D);
   glPopMatrix();

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);
}

// Draw the greenhouse base, vertical posts, eave rails, ridge, and roof rafters.
// Every member is a transformed box, so its six outward normals remain valid.
void drawGreenhouseFrame()
{
   const double roofAngle = 32.735;
   const double roofLength = std::sqrt(1.4 * 1.4 + 0.9 * 0.9);
   const double frameX[] = {-2.0, 0.0, 2.0};

   glPushMatrix();
   glTranslated(-4.2, 0, 3.4);
   glColor3f(0.62f, 0.68f, 0.64f);

   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, textureMetal);
   }

   drawBox(0, 0.08, -1.4, 4.2, 0.16, 0.14);
   drawBox(0, 0.08,  1.4, 4.2, 0.16, 0.14);
   drawBox(-2.0, 0.08, 0, 0.14, 0.16, 2.8);
   drawBox( 2.0, 0.08, 0, 0.14, 0.16, 2.8);

   for (int i = 0; i < 3; ++i)
   {
      drawBox(frameX[i], 0.82, -1.4, 0.12, 1.5, 0.12);
      drawBox(frameX[i], 0.82,  1.4, 0.12, 1.5, 0.12);
   }

   drawBox(0, 1.55, -1.4, 4.2, 0.12, 0.12);
   drawBox(0, 1.55,  1.4, 4.2, 0.12, 0.12);
   drawBox(0, 2.45, 0, 4.2, 0.12, 0.12);

   for (int i = 0; i < 3; ++i)
   {
      glPushMatrix();
      glTranslated(frameX[i], 2.0, -0.7);
      glRotated(-roofAngle, 1, 0, 0);
      glScaled(0.12, 0.12, roofLength);
      drawBoxUnit();
      glPopMatrix();

      glPushMatrix();
      glTranslated(frameX[i], 2.0, 0.7);
      glRotated(roofAngle, 1, 0, 0);
      glScaled(0.12, 0.12, roofLength);
      drawBoxUnit();
      glPopMatrix();
   }

   glDisable(GL_TEXTURE_2D);
   glPopMatrix();
}

// Draw the greenhouse wall and roof glass geometry.
// Blending and depth-write state are configured by the transparent scene pass.
void drawGreenhouseGlassPanels()
{
   const double roofAngle = 32.735;
   const double roofLength = std::sqrt(1.4 * 1.4 + 0.9 * 0.9);
   const float glassSpecular[] = {0.70f, 0.82f, 0.86f, 1.0f};
   const float defaultSpecular[] = {0.22f, 0.22f, 0.22f, 1.0f};

   glDisable(GL_TEXTURE_2D);
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, glassSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 64.0f);
   glPushMatrix();
   glTranslated(-4.2, 0, 3.4);
   glColor4f(0.48f, 0.76f, 0.78f, 0.38f);

   for (int side = -1; side <= 1; side += 2)
   {
      drawBox(-1.0, 0.82, 1.405 * side, 1.82, 1.32, 0.035);
      drawBox( 1.0, 0.82, 1.405 * side, 1.82, 1.32, 0.035);
      drawBox(2.005 * side, 0.82, -0.7, 0.035, 1.32, 1.22);
      drawBox(2.005 * side, 0.82,  0.7, 0.035, 1.32, 1.22);
   }

   for (int section = -1; section <= 1; section += 2)
   {
      glPushMatrix();
      glTranslated(section, 2.0, -0.7);
      glRotated(-roofAngle, 1, 0, 0);
      glScaled(1.82, 0.035, roofLength - 0.14);
      drawBoxUnit();
      glPopMatrix();

      glPushMatrix();
      glTranslated(section, 2.0, 0.7);
      glRotated(roofAngle, 1, 0, 0);
      glScaled(1.82, 0.035, roofLength - 0.14);
      drawBoxUnit();
      glPopMatrix();
   }

   glPopMatrix();
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);
}

// Draw a dark photovoltaic panel as a shallow handmade box.
// The top normal is transformed with the panel angle by OpenGL's normal matrix.
void drawSolarPanelUnit()
{
   const float panelSpecular[] = {0.58f, 0.68f, 0.82f, 1.0f};
   const float defaultSpecular[] = {0.22f, 0.22f, 0.22f, 1.0f};

   glDisable(GL_TEXTURE_2D);
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, panelSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 72.0f);

   glColor3f(0.04f, 0.12f, 0.24f);
   drawBox(0, 0, 0, 2.15, 0.08, 1.25);

   glDisable(GL_LIGHTING);
   glColor3f(0.30f, 0.55f, 0.78f);
   glBegin(GL_LINES);
   glNormal3f(0, 1, 0);
   for (int i = -1; i <= 1; ++i)
   {
      glVertex3d(0.52 * i, 0.045, -0.60);
      glVertex3d(0.52 * i, 0.045,  0.60);
   }
   glVertex3d(-1.05, 0.045, 0);
   glVertex3d( 1.05, 0.045, 0);
   glEnd();
   if (lighting)
      glEnable(GL_LIGHTING);

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);
}

// Draw a compact array of angled panels with handmade support posts and braces.
void drawSolarPanelArray()
{
   const double panelX[] = {-1.25, 1.25};
   const double panelZ[] = {-0.8, 0.8};

   glPushMatrix();
   glTranslated(3.3, 0, -0.2);
   glRotated(-12, 0, 1, 0);

   glColor3f(0.42f, 0.44f, 0.46f);
   for (int row = 0; row < 2; ++row)
   {
      for (int column = 0; column < 2; ++column)
      {
         const double x = panelX[column];
         const double z = panelZ[row];
         drawBox(x - 0.72, 0.48, z, 0.10, 0.96, 0.10);
         drawBox(x + 0.72, 0.48, z, 0.10, 0.96, 0.10);

         glPushMatrix();
         glTranslated(x, 1.02, z);
         glRotated(-24, 1, 0, 0);
         drawSolarPanelUnit();
         glPopMatrix();
      }
   }

   glPopMatrix();
}

// Draw one low-poly tree with a box trunk and two eight-sided cone canopies.
// Cone side normals include both radial and upward components for lighting.
void drawTree(double x, double z, double scale)
{
   const int sides = 8;

   glPushMatrix();
   glTranslated(x, 0, z);
   glScaled(scale, scale, scale);

   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, textureWood);
   }
   glColor3f(0.38f, 0.22f, 0.10f);
   drawBox(0, 0.65, 0, 0.28, 1.3, 0.28);
   glDisable(GL_TEXTURE_2D);

   for (int layer = 0; layer < 2; ++layer)
   {
      const double baseY = 0.75 + 0.55 * layer;
      const double height = 1.25;
      const double radius = 0.85 - 0.18 * layer;
      const double normalLength = std::sqrt(height * height + radius * radius);

      glColor3f(0.16f + 0.03f * layer, 0.42f + 0.06f * layer, 0.15f);
      glBegin(GL_TRIANGLES);
      for (int i = 0; i < sides; ++i)
      {
         const double angle = 360.0 * i / sides;
         const double nextAngle = 360.0 * (i + 1) / sides;
         const double middleAngle = 0.5 * (angle + nextAngle);
         glNormal3d(height * Cos(middleAngle) / normalLength,
                    radius / normalLength,
                    height * Sin(middleAngle) / normalLength);
         glVertex3d(0, baseY + height, 0);
         glVertex3d(radius * Cos(nextAngle), baseY, radius * Sin(nextAngle));
         glVertex3d(radius * Cos(angle), baseY, radius * Sin(angle));
      }
      glEnd();
   }

   glPopMatrix();
}

// Place a small number of trees around the boundary without obscuring turbines.
void drawTreeGroup()
{
   drawTree(-6.8, 4.5, 0.85);
   drawTree(-7.0, 1.8, 0.70);
   drawTree( 7.0, 0.8, 0.78);
   drawTree( 6.7, -4.8, 0.68);
}

// Draw all wind turbines using the reusable handmade turbine model.
void drawTurbineGroup()
{
   const Instance windmills[] =
   {
      {-1.0, 0.0,  0.5, 1.35, 1.35, 1.35,  10, 0.72f, 0.72f, 0.68f,  12},
      {-5.1, 0.0, -2.3, 0.82, 0.95, 0.82, -28, 0.48f, 0.62f, 0.70f,  48},
      { 4.2, 0.0, -3.8, 0.62, 0.72, 0.62,  37, 0.72f, 0.58f, 0.42f, -25}
   };

   const int count = sizeof(windmills) / sizeof(windmills[0]);
   for (int i = 0; i < count; ++i)
      drawWindmillInstance(windmills[i]);
}

// Draw the barn/farmhouse object group.
void drawBarnGroup()
{
   drawBarnOrShed();
}

// Draw only the opaque greenhouse base, posts, rails, ridge, and rafters.
void drawGreenhouseOpaque()
{
   drawGreenhouseFrame();
}

// Draw transparent greenhouse glass after every opaque object.
void drawGreenhouseTransparent()
{
   if (!glassVisible)
      return;

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   // Keep depth testing enabled so opaque surfaces still occlude the glass.
   // Disable only depth writes so glass does not hide objects behind it.
   glDepthMask(GL_FALSE);
   drawGreenhouseGlassPanels();
   glDepthMask(GL_TRUE);

   glDisable(GL_BLEND);
}

// -----------------------------------------------------------------------------
// Wind ribbons and GLSL shader setup
// -----------------------------------------------------------------------------

// Read an entire GLSL source file. Missing files are non-fatal because the
// ribbons have a fixed-pipeline fallback.
bool ReadTextFile(const char* file, std::string& text)
{
   std::ifstream input(file);
   if (!input)
   {
      std::fprintf(stderr, "Shader error: cannot open %s\n", file);
      return false;
   }

   text.assign(std::istreambuf_iterator<char>(input),
               std::istreambuf_iterator<char>());
   if (!input.good() && !input.eof())
   {
      std::fprintf(stderr, "Shader error: cannot read %s\n", file);
      text.clear();
      return false;
   }
   return true;
}

// Print a shader compiler log when compilation fails.
void PrintShaderLog(GLuint shader, const char* file)
{
   GLint length = 0;
   glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
   if (length <= 1)
      return;

   std::vector<GLchar> log(length);
   glGetShaderInfoLog(shader, length, 0, &log[0]);
   std::fprintf(stderr, "Shader compile log (%s):\n%s\n", file, &log[0]);
}

// Print a program linker log when linking fails.
void PrintProgramLog(GLuint program)
{
   GLint length = 0;
   glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
   if (length <= 1)
      return;

   std::vector<GLchar> log(length);
   glGetProgramInfoLog(program, length, 0, &log[0]);
   std::fprintf(stderr, "Shader link log:\n%s\n", &log[0]);
}

// Compile one GLSL stage, report its diagnostics, and return zero on failure.
GLuint CompileShader(GLenum type, const char* file)
{
   std::string source;
   if (!ReadTextFile(file, source))
      return 0;

   const GLchar* sourcePointer = source.c_str();
   const GLuint shader = glCreateShader(type);
   if (!shader)
   {
      std::fprintf(stderr, "Shader error: glCreateShader failed for %s\n", file);
      return 0;
   }

   glShaderSource(shader, 1, &sourcePointer, 0);
   glCompileShader(shader);

   GLint compiled = GL_FALSE;
   glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
   if (!compiled)
   {
      PrintShaderLog(shader, file);
      glDeleteShader(shader);
      return 0;
   }
   return shader;
}

// Compile and link the wind vertex/fragment stages. Failure is non-fatal so
// the rest of the scene remains usable with unshaded translucent ribbons.
GLuint CreateShaderProgram(const char* vertexFile, const char* fragmentFile)
{
   const GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexFile);
   if (!vertexShader)
      return 0;

   const GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentFile);
   if (!fragmentShader)
   {
      glDeleteShader(vertexShader);
      return 0;
   }

   const GLuint program = glCreateProgram();
   if (!program)
   {
      std::fprintf(stderr, "Shader error: glCreateProgram failed\n");
      glDeleteShader(vertexShader);
      glDeleteShader(fragmentShader);
      return 0;
   }

   glAttachShader(program, vertexShader);
   glAttachShader(program, fragmentShader);
   glLinkProgram(program);

   GLint linked = GL_FALSE;
   glGetProgramiv(program, GL_LINK_STATUS, &linked);
   glDetachShader(program, vertexShader);
   glDetachShader(program, fragmentShader);
   glDeleteShader(vertexShader);
   glDeleteShader(fragmentShader);

   if (!linked)
   {
      PrintProgramLog(program);
      glDeleteProgram(program);
      return 0;
   }
   return program;
}

// Verify the shader uniforms once at startup without affecting scene drawing.
void CheckWindShader()
{
   if (!shaderEnabled || !windProgram)
      return;

   glUseProgram(windProgram);
   const GLint timeLocation = glGetUniformLocation(windProgram, "time");
   const GLint windSpeedLocation = glGetUniformLocation(windProgram, "windSpeed");
   if (timeLocation >= 0)
      glUniform1f(timeLocation, 0.0f);
   if (windSpeedLocation >= 0)
      glUniform1f(windSpeedLocation, static_cast<float>(windSpeed));
   glUseProgram(0);
}

// Draw handmade translucent wind-flow strips across the turbine field.
// Only this function activates the wind shader; every exit restores program 0.
// Texture coordinates carry along-strip phase and per-ribbon phase to GLSL.
void drawWindRibbons()
{
   if (!windFlowVisible || (inspectionMode != 0 && inspectionMode != 1))
      return;

   const int ribbonCount = 12;
   const int segmentCount = 40;
   const double ribbonZ[ribbonCount] =
   {
      -4.8, -4.0, -3.3, -2.6, -1.8, -1.0,
      -0.2,  0.6,  1.4,  2.2,  3.1,  4.0
   };

   glDisable(GL_LIGHTING);
   glDisable(GL_TEXTURE_2D);
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDepthMask(GL_FALSE);

   const bool useShader = shaderEnabled && windProgram;
   if (useShader)
   {
      glUseProgram(windProgram);

      // The same windSpeed that advances turbine blades controls shader travel.
      const float elapsedSeconds =
         static_cast<float>(glutGet(GLUT_ELAPSED_TIME)) / 1000.0f;
      const GLint timeLocation = glGetUniformLocation(windProgram, "time");
      const GLint windSpeedLocation =
         glGetUniformLocation(windProgram, "windSpeed");
      const GLint fogEnabledLocation =
         glGetUniformLocation(windProgram, "fogEnabled");
      const GLint fogColorLocation =
         glGetUniformLocation(windProgram, "fogColor");
      const GLint fogStartLocation =
         glGetUniformLocation(windProgram, "fogStart");
      const GLint fogEndLocation =
         glGetUniformLocation(windProgram, "fogEnd");
      if (timeLocation >= 0)
         glUniform1f(timeLocation, elapsedSeconds);
      if (windSpeedLocation >= 0)
         glUniform1f(windSpeedLocation, static_cast<float>(windSpeed));
      // GLSL does not receive fixed-function fog automatically, so pass the
      // same linear-fog parameters used by opaque and transparent fixed draws.
      if (fogEnabledLocation >= 0)
         glUniform1i(fogEnabledLocation, fogEnabled);
      if (fogColorLocation >= 0)
         glUniform4fv(fogColorLocation, 1, fogColor);
      if (fogStartLocation >= 0)
         glUniform1f(fogStartLocation, fogStart);
      if (fogEndLocation >= 0)
         glUniform1f(fogEndLocation, fogEnd);
   }

   for (int ribbon = 0; ribbon < ribbonCount; ++ribbon)
   {
      const double ribbonPhase = 0.73 * ribbon;
      const double centerY = 0.75 + 0.22 * (ribbon % 7);
      const double halfWidth = 0.055 + 0.012 * (ribbon % 3);

      // The fallback retains readable curved airflow when GLSL is unavailable.
      glColor4f(0.58f, 0.84f, 1.0f, useShader ? 1.0f : 0.24f);
      glBegin(GL_QUAD_STRIP);
      for (int segment = 0; segment <= segmentCount; ++segment)
      {
         const double along =
            static_cast<double>(segment) / segmentCount;
         const double x = -8.5 + 17.0 * along;
         const double curve =
            0.32 * std::sin(0.55 * x + ribbonPhase);
         const double y =
            centerY + 0.08 * std::sin(0.9 * x + ribbonPhase);
         const double z = ribbonZ[ribbon] + curve;

         glTexCoord2d(along, ribbonPhase);
         glVertex3d(x, y - halfWidth, z);
         glTexCoord2d(along, ribbonPhase);
         glVertex3d(x, y + halfWidth, z);
      }
      glEnd();
   }

   // Never allow the ribbon shader to leak into glass, HUD, or later frames.
   glUseProgram(0);

   glDepthMask(GL_TRUE);
   glDisable(GL_BLEND);
}

// -----------------------------------------------------------------------------
// Scene dispatch
// -----------------------------------------------------------------------------

// Draw the complete handmade solar-energy object group.
void drawSolarGroup()
{
   drawSolarPanelArray();
}

// Draw environmental and secondary objects used around the energy farm.
void drawSecondaryObjects()
{
   drawGround();
   drawPath();
   drawFence();
   drawTreeGroup();
}

// Draw every opaque object group in the main farm scene.
void drawFullSceneOpaque()
{
   drawSecondaryObjects();
   drawBarnGroup();
   drawTurbineGroup();
   drawGreenhouseOpaque();
   drawSolarGroup();
}

// Dispatch opaque scene rendering according to the active inspection selection.
void drawInspectedOpaqueObjects()
{
   switch (inspectionMode)
   {
      case 0: drawFullSceneOpaque();  break;
      case 1: drawTurbineGroup();     break;
      case 2: drawBarnGroup();        break;
      case 3: drawGreenhouseOpaque(); break;
      case 4: drawSolarGroup();       break;
      case 5: drawSecondaryObjects(); break;
   }
}

// Draw transparent objects after opaque geometry. Wind renders before glass so
// the greenhouse glazing can tint airflow that visually passes behind it.
void drawInspectedTransparentObjects()
{
   drawWindRibbons();

   if (inspectionMode == 0 || inspectionMode == 3)
      drawGreenhouseTransparent();
}

// Draw a visible marker at the moving positional light source.
void drawLightMarker(double x, double y, double z)
{
   glDisable(GL_LIGHTING);
   glDisable(GL_TEXTURE_2D);
   glColor3f(1.0f, 0.85f, 0.20f);
   glPushMatrix();
   glTranslated(x, y, z);
   glScaled(0.3, 0.3, 0.3);
   drawBoxUnit();
   glPopMatrix();
}

// -----------------------------------------------------------------------------
// Lighting, fog, and transparency state
// -----------------------------------------------------------------------------

// Configure the fixed-pipeline positional light and material defaults.
void ConfigureLighting(const float position[4])
{
   const float ambientLight[] = {0.20f, 0.20f, 0.22f, 1.0f};
   const float diffuseLight[] = {0.82f, 0.78f, 0.68f, 1.0f};
   const float specularLight[] = {0.45f, 0.43f, 0.38f, 1.0f};
   const float materialAmbient[] = {0.25f, 0.25f, 0.25f, 1.0f};
   const float materialDiffuse[] = {0.80f, 0.80f, 0.80f, 1.0f};
   const float materialSpecular[] = {0.22f, 0.22f, 0.22f, 1.0f};

   glEnable(GL_NORMALIZE);
   glEnable(GL_LIGHTING);
   glEnable(GL_LIGHT0);
   glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
   glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
   glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
   glLightfv(GL_LIGHT0, GL_POSITION, position);

   glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, materialAmbient);
   glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, materialDiffuse);
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);

   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
   glEnable(GL_COLOR_MATERIAL);
}

// Configure subtle linear distance haze for all fixed-pipeline 3D geometry.
// The color complements the clear color, while the distant end preserves the
// full farm and leaves close inspection modes essentially unaffected.
void ConfigureFog()
{
   if (!fogEnabled)
   {
      glDisable(GL_FOG);
      return;
   }

   glEnable(GL_FOG);
   glFogi(GL_FOG_MODE, GL_LINEAR);
   glFogfv(GL_FOG_COLOR, fogColor);
   glFogf(GL_FOG_START, fogStart);
   glFogf(GL_FOG_END, fogEnd);
   glHint(GL_FOG_HINT, GL_NICEST);
}

// -----------------------------------------------------------------------------
// GLUT callbacks
// -----------------------------------------------------------------------------

// Render the selected object group, scene overlays, and status HUD.
void display()
{
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glEnable(GL_DEPTH_TEST);

   Project();

   if (mode == 0)
   {
      glRotated(ph, 1, 0, 0);
      glRotated(th, 0, 1, 0);
      glTranslated(-viewTargetX, -viewTargetY, -viewTargetZ);
   }
   else if (mode == 1)
   {
      const double ex = viewTargetX - 2 * dim * Sin(th) * Cos(ph);
      const double ey = viewTargetY + 2 * dim * Sin(ph);
      const double ez = viewTargetZ + 2 * dim * Cos(th) * Cos(ph);
      gluLookAt(ex, ey, ez,
                viewTargetX, viewTargetY, viewTargetZ,
                0, Cos(ph), 0);
   }
   else
   {
      // Look one unit ahead in the current first-person heading.
      const double lookX = fpX + Sin(fpYaw);
      const double lookZ = fpZ - Cos(fpYaw);
      gluLookAt(fpX, fpY, fpZ, lookX, fpY, lookZ, 0, 1, 0);
   }

   ConfigureFog();

   const float lightPosition[] =
   {
      static_cast<float>(7 * Cos(lightAngle)),
      static_cast<float>(lightHeight),
      static_cast<float>(7 * Sin(lightAngle)),
      1.0f
   };
   if (inspectionMode == 0)
      drawLightMarker(lightPosition[0], lightPosition[1], lightPosition[2]);
   if (lighting)
      ConfigureLighting(lightPosition);
   else
      glDisable(GL_LIGHTING);

   drawInspectedOpaqueObjects();
   glDisable(GL_LIGHTING);
   if (axes)
      DrawAxes();

   if (lighting)
      glEnable(GL_LIGHTING);

   // Transparent glass is drawn after all opaque scene geometry so the
   // existing depth buffer can reject hidden fragments without glass writing
   // new depth values that would incorrectly conceal objects behind it.
   drawInspectedTransparentObjects();

   // Fog is a 3D scene effect; disable it before the screen-space HUD and
   // restore it from fogEnabled at the start of the next frame.
   glDisable(GL_FOG);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_LIGHTING);
   glMatrixMode(GL_PROJECTION);
   glPushMatrix();
   glLoadIdentity();
   glOrtho(0, windowWidth, 0, windowHeight, -1, 1);
   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   glLoadIdentity();

   glColor3f(1, 1, 1);
   char viewText[100];
   if (mode == 2)
      std::snprintf(viewText, sizeof(viewText), "View angle: yaw=%d", fpYaw);
   else
      std::snprintf(viewText, sizeof(viewText), "View angle: th=%d ph=%d", th, ph);

   char lightText[100];
   std::snprintf(lightText, sizeof(lightText),
                 "Light: angle=%.0f height=%.1f movement=%s lighting=%s",
                 lightAngle, lightHeight, moveLight ? "running" : "paused",
                 lighting ? "on" : "off");

   char stateText[240];
   std::snprintf(stateText, sizeof(stateText),
                 "Inspection: %s   Lighting: %s   Fog: %s   Glass: %s   Shader: %s   Wind Flow: %s",
                 InspectionName(), lighting ? "on" : "off",
                 fogEnabled ? "On" : "Off", glassVisible ? "on" : "off",
                 shaderEnabled && windProgram ? "On" : "Off",
                 windFlowVisible ? "On" : "Off");

   const double bladeRpm =
      rotateBlades ? baseBladeDegreesPerSecond * windSpeed / 6.0 : 0.0;
   char windText[100];
   std::snprintf(windText, sizeof(windText),
                 "Wind Speed: %.2f   Turbine RPM: %.2f   Blades: %s",
                 windSpeed, bladeRpm, rotateBlades ? "running" : "paused");

   DrawText(10, 170, "Shader-Based Renewable Energy Farm Visualization");
   DrawText(10, 150, stateText);
   DrawText(10, 130, windText);
   DrawText(10, 110, lightText);
   DrawText(10, 90, viewText);
   DrawText(10, 70, ModeName());
   DrawText(10, 50, "0-5: inspect  arrows: navigate  l: light  f: fog  t: textures  g: glass  S: wind flow  [ / ]: wind");
   DrawText(10, 30, "r: blades  R: reset camera  SPACE: pause light  ,/.: light angle  </>: light height");
   DrawText(10, 10,
            "m: camera mode  +/- or PgUp/PgDn: zoom/FOV  a: axes  q/ESC: exit");

   glPopMatrix();
   glMatrixMode(GL_PROJECTION);
   glPopMatrix();
   glMatrixMode(GL_MODELVIEW);

   ErrCheck("display");
   glutSwapBuffers();
}

// Maintain the viewport and projection when the window dimensions change.
void reshape(int width, int height)
{
   windowWidth = width;
   windowHeight = height > 0 ? height : 1;
   asp = static_cast<double>(windowWidth) / windowHeight;
   glViewport(0, 0, windowWidth, windowHeight);
   Project();
}

// Apply a centered overview preset for the selected inspection group.
void SetInspectionMode(int selectedMode)
{
   inspectionMode = selectedMode;
   mode = 1;

   switch (inspectionMode)
   {
      case 0:
         viewTargetX = 0;
         viewTargetY = 1;
         viewTargetZ = 0;
         th = 35;
         ph = 25;
         dim = 9;
         break;
      case 1:
         viewTargetX = -0.5;
         viewTargetY = 2.0;
         viewTargetZ = -1.0;
         th = 25;
         ph = 15;
         dim = 6;
         break;
      case 2:
         viewTargetX = 5.7;
         viewTargetY = 1.2;
         viewTargetZ = 3.4;
         th = 35;
         ph = 15;
         dim = 3;
         break;
      case 3:
         viewTargetX = -3.5;
         viewTargetY = 1.0;
         viewTargetZ = 3.2;
         th = 30;
         ph = 18;
         dim = 3;
         break;
      case 4:
         viewTargetX = 3.0;
         viewTargetY = 0.8;
         viewTargetZ = -1.0;
         th = 30;
         ph = 22;
         dim = 4;
         break;
      case 5:
         viewTargetX = 0;
         viewTargetY = 0.5;
         viewTargetZ = 0;
         th = 35;
         ph = 30;
         dim = 9;
         break;
   }
}

// Restore the original full-scene camera and moving-light position.
void ResetCamera()
{
   SetInspectionMode(0);
   fov = 60;
   fpX = 0;
   fpY = 1;
   fpZ = 12;
   fpYaw = 0;
   lightAngle = 90;
   lightHeight = 5;
}

// Adjust orthographic size or perspective field of view.
void AdjustZoom(int direction)
{
   if (mode == 0)
   {
      dim -= 0.5 * direction;
      if (dim < 3)
         dim = 3;
      if (dim > 20)
         dim = 20;
   }
   else
   {
      fov -= 5 * direction;
      if (fov < 20)
         fov = 20;
      if (fov > 100)
         fov = 100;
   }
}

// Handle normal-key controls and object inspection selection.
void key(unsigned char ch, int, int)
{
   if (ch == 27 || ch == 'q' || ch == 'Q')
   {
      std::exit(0);
   }
   else if (ch == 'm' || ch == 'M')
   {
      mode = (mode + 1) % 3;
   }
   else if (ch == 'a' || ch == 'A')
   {
      axes = 1 - axes;
   }
   else if (ch == 'r')
   {
      rotateBlades = 1 - rotateBlades;
   }
   else if (ch == 'R')
   {
      ResetCamera();
   }
   else if (ch == 't' || ch == 'T')
   {
      textures = 1 - textures;
   }
   else if (ch == 'g' || ch == 'G')
   {
      glassVisible = 1 - glassVisible;
   }
   else if (ch == 's' || ch == 'S')
   {
      windFlowVisible = 1 - windFlowVisible;
      shaderEnabled = windFlowVisible && windProgram;
   }
   else if (ch == 'l' || ch == 'L')
   {
      lighting = 1 - lighting;
   }
   else if (ch == 'f' || ch == 'F')
   {
      fogEnabled = 1 - fogEnabled;
   }
   else if (ch == ' ')
   {
      moveLight = 1 - moveLight;
   }
   else if (ch == ',')
   {
      lightAngle = std::fmod(lightAngle - 5 + 360, 360.0);
   }
   else if (ch == '.')
   {
      lightAngle = std::fmod(lightAngle + 5, 360.0);
   }
   else if (ch == '[')
   {
      windSpeed -= windSpeedStep;
      if (windSpeed < minWindSpeed)
         windSpeed = minWindSpeed;
   }
   else if (ch == ']')
   {
      windSpeed += windSpeedStep;
      if (windSpeed > maxWindSpeed)
         windSpeed = maxWindSpeed;
   }
   else if (ch == '<')
   {
      lightHeight -= 0.25;
      if (lightHeight < 0.5)
         lightHeight = 0.5;
   }
   else if (ch == '>')
   {
      lightHeight += 0.25;
      if (lightHeight > 12)
         lightHeight = 12;
   }
   else if (ch >= '0' && ch <= '5')
   {
      SetInspectionMode(ch - '0');
   }
   else if (ch == '+' || ch == '=')
   {
      AdjustZoom(1);
   }
   else if (ch == '-' || ch == '_')
   {
      AdjustZoom(-1);
   }

   Project();
   glutPostRedisplay();
}

// Handle arrow-key camera navigation and page-key zoom controls.
void special(int key, int, int)
{
   if (key == GLUT_KEY_PAGE_UP)
   {
      AdjustZoom(1);
      Project();
      glutPostRedisplay();
      return;
   }
   if (key == GLUT_KEY_PAGE_DOWN)
   {
      AdjustZoom(-1);
      Project();
      glutPostRedisplay();
      return;
   }

   if (mode == 2)
   {
      if (key == GLUT_KEY_LEFT)
         fpYaw = (fpYaw - 5) % 360;
      else if (key == GLUT_KEY_RIGHT)
         fpYaw = (fpYaw + 5) % 360;
      else if (key == GLUT_KEY_UP)
      {
         fpX += 0.2 * Sin(fpYaw);
         fpZ -= 0.2 * Cos(fpYaw);
      }
      else if (key == GLUT_KEY_DOWN)
      {
         fpX -= 0.2 * Sin(fpYaw);
         fpZ += 0.2 * Cos(fpYaw);
      }

   }
   else
   {
      if (key == GLUT_KEY_LEFT)
         th -= 5;
      else if (key == GLUT_KEY_RIGHT)
         th += 5;
      else if (key == GLUT_KEY_UP)
         ph += 5;
      else if (key == GLUT_KEY_DOWN)
         ph -= 5;

      th %= 360;
      if (ph > 85)
         ph = 85;
      if (ph < -85)
         ph = -85;
   }

   glutPostRedisplay();
}

// Advance animation using elapsed time so speed is frame-rate independent.
void idle()
{
   static int previousTime = glutGet(GLUT_ELAPSED_TIME);
   const int currentTime = glutGet(GLUT_ELAPSED_TIME);
   const int elapsed = currentTime - previousTime;
   previousTime = currentTime;

   // The wind model scales the original 45-degree/second blade speed linearly.
   // Elapsed wall-clock time keeps motion smooth and independent of frame rate.
   if (rotateBlades)
   {
      const double elapsedSeconds = elapsed / 1000.0;
      const double bladeDegrees =
         baseBladeDegreesPerSecond * windSpeed * elapsedSeconds;
      bladeAngle = std::fmod(bladeAngle + bladeDegrees, 360.0);
   }
   if (moveLight)
      lightAngle = std::fmod(lightAngle + 0.025 * elapsed, 360.0);

   glutPostRedisplay();
}

// -----------------------------------------------------------------------------
// Initialization and main
// -----------------------------------------------------------------------------

// Initialize GLUT, load textures, and enter the event loop.
int main(int argc, char* argv[])
{
   glutInit(&argc, argv);
   // GLUT_DEPTH allocates the depth buffer used for hidden-surface removal.
   glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
   glutInitWindowSize(windowWidth, windowHeight);
   glutCreateWindow("Gunabhiram Aruru - Shader-Based Renewable Energy Farm Visualization");

#ifdef USEGLEW
   glewExperimental = GL_TRUE;
   const GLenum glewStatus = glewInit();
   if (glewStatus != GLEW_OK)
   {
      std::fprintf(stderr, "GLEW initialization failed: %s\n",
                   glewGetErrorString(glewStatus));
      shaderEnabled = 0;
   }
   // GLEW can generate GL_INVALID_ENUM while probing a compatibility context.
   glGetError();
#endif

   glutDisplayFunc(display);
   glutReshapeFunc(reshape);
   glutKeyboardFunc(key);
   glutSpecialFunc(special);
   glutIdleFunc(idle);

   glClearColor(0.07f, 0.09f, 0.12f, 1.0f);
   // The depth test makes nearer solid surfaces hide farther surfaces.
   glEnable(GL_DEPTH_TEST);
   glEnable(GL_NORMALIZE);

   textureGrass = LoadTexBMP("textures/grass.bmp");
   textureWood = LoadTexBMP("textures/wood.bmp");
   textureRoof = LoadTexBMP("textures/roof.bmp");
   texturePath = LoadTexBMP("textures/path.bmp");
   textureMetal = LoadTexBMP("textures/metal.bmp");

   if (shaderEnabled)
   {
      windProgram = CreateShaderProgram("wind.vert", "wind.frag");
      if (windProgram)
      {
         CheckWindShader();
         std::fprintf(stdout, "Wind shader compiled and linked successfully.\n");
      }
      else
      {
         shaderEnabled = 0;
         std::fprintf(stderr,
                      "Wind shader unavailable; using fixed-pipeline ribbons.\n");
      }
   }

   glutMainLoop();
   return 0;
}
