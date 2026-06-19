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
const double anemometerDegreesPerSecond = 120.0;
const double terrainHalfSize = 45.0;
const float fogColor[] = {0.12f, 0.16f, 0.20f, 1.0f};
const float fogStart = 52.0f;
const float fogEnd = 115.0f;

// Zone centers keep the large farm layout explicit and easy to extend.
const double windZoneX = -22.0;
const double windZoneZ = -18.0;
const double solarZoneX = 18.0;
const double solarZoneZ = -18.0;
const double batteryZoneX = 29.0;
const double batteryZoneZ = -7.0;
const double substationZoneX = 28.0;
const double substationZoneZ = 13.0;
const double barnZoneX = 20.0;
const double barnZoneZ = 27.0;
const double greenhouseZoneX = -5.0;
const double greenhouseZoneZ = 25.0;
const double paddockZoneX = -25.0;
const double paddockZoneZ = 24.0;

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
double lightHeight = 25;
int th = 35;
int ph = 30;
int fov = 60;
double asp = 1;
double dim = 42;
double bladeAngle = 0;
double anemometerAngle = 0;
double fpX = 0;
double fpY = 1;
double fpZ = 42;
int fpYaw = 0;
double viewTargetX = 0;
double viewTargetY = 1;
double viewTargetZ = 0;
int windowWidth = 1200;
int windowHeight = 800;
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

// Return the exact object represented by the active 0-9 inspection key.
const char* InspectionName()
{
   switch (inspectionMode)
   {
      case 0: return "Full scene";
      case 1: return "Wind turbine";
      case 2: return "Solar panel";
      case 3: return "Battery unit";
      case 4: return "Substation";
      case 5: return "Greenhouse";
      case 6: return "Control building";
      case 7: return "Weather station";
      case 8: return "Sheep";
      default: return "Farmer";
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

// Build the turbine support from a square footing, a smooth custom tapered
// cylinder, and a box nacelle. Per-face normals account for the tower taper so
// fixed-pipeline lighting remains correct.
void drawWindmillBaseUnit()
{
   const int towerSides = 16;
   const double bottomY = 0.24;
   const double topY = 2.85;
   const double bottomRadius = 0.35;
   const double topRadius = 0.14;
   const double towerHeight = topY - bottomY;
   const double taper = (bottomRadius - topRadius) / towerHeight;
   const double normalLength = std::sqrt(1.0 + taper * taper);

   glPushMatrix();
   glTranslated(0, 0.12, 0);
   glScaled(0.9, 0.24, 0.9);
   drawBoxUnit(2, 1, 2);
   glPopMatrix();

   glBegin(GL_QUADS);
   for (int side = 0; side < towerSides; ++side)
   {
      const double angle0 = 360.0 * side / towerSides;
      const double angle1 = 360.0 * (side + 1) / towerSides;
      const double normalAngle = 0.5 * (angle0 + angle1);
      glNormal3d(Cos(normalAngle) / normalLength,
                 taper / normalLength,
                 Sin(normalAngle) / normalLength);
      glTexCoord2d(static_cast<double>(side) / towerSides, 0);
      glVertex3d(bottomRadius * Cos(angle0), bottomY,
                 bottomRadius * Sin(angle0));
      glTexCoord2d(static_cast<double>(side + 1) / towerSides, 0);
      glVertex3d(bottomRadius * Cos(angle1), bottomY,
                 bottomRadius * Sin(angle1));
      glTexCoord2d(static_cast<double>(side + 1) / towerSides, 3);
      glVertex3d(topRadius * Cos(angle1), topY,
                 topRadius * Sin(angle1));
      glTexCoord2d(static_cast<double>(side) / towerSides, 3);
      glVertex3d(topRadius * Cos(angle0), topY,
                 topRadius * Sin(angle0));
   }
   glEnd();

   // drawBoxUnit supplies outward normals for all six nacelle faces.
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
void drawTurbine(double bladeOffset = 0.0)
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
   for (int blade = 0; blade < 3; ++blade)
   {
      glPushMatrix();
      glRotated(120 * blade, 0, 0, 1);
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
   drawTurbine(instance.bladeAngle);
   glPopMatrix();
}

// Draw the expanded terrain around the origin, including the textured grass
// surface, a low soil foundation, and handmade crop rows at the outer edges.
void drawExpandedTerrain()
{
   glPushMatrix();
   glTranslated(0, -0.12, 0);
   glScaled(2 * terrainHalfSize, 0.2, 2 * terrainHalfSize);
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
      glTexCoord2f(0, 0);   glVertex3d(-terrainHalfSize, -0.019, -terrainHalfSize);
      glTexCoord2f(30, 0);  glVertex3d( terrainHalfSize, -0.019, -terrainHalfSize);
      glTexCoord2f(30, 30); glVertex3d( terrainHalfSize, -0.019,  terrainHalfSize);
      glTexCoord2f(0, 30);  glVertex3d(-terrainHalfSize, -0.019,  terrainHalfSize);
      glEnd();
      glDisable(GL_TEXTURE_2D);
   }

   // Crop rows occupy unused corners and reinforce the agricultural purpose.
   glColor3f(0.33f, 0.24f, 0.13f);
   for (int row = 0; row < 10; ++row)
   {
      drawBox(-39.0 + 0.75 * row, 0.025, 36.5, 0.34, 0.05, 12.0);
      drawBox( 32.0 + 0.75 * row, 0.025, -36.5, 0.34, 0.05, 12.0);
   }

   // A thin drainage border prevents the large ground slab from reading as
   // an unbounded plane while remaining above the base without z-fighting.
   glColor3f(0.18f, 0.32f, 0.15f);
   drawBox(0, 0.015, -44.0, 88.0, 0.03, 0.32);
   drawBox(0, 0.015,  44.0, 88.0, 0.03, 0.32);
   drawBox(-44.0, 0.015, 0, 0.32, 0.03, 88.0);
   drawBox( 44.0, 0.015, 0, 0.32, 0.03, 88.0);
}

// Draw one textured road strip above the grass surface.
void drawRoadStrip(double x, double z, double length, double width,
                   double rotation)
{
   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, texturePath);
      glColor3f(1, 1, 1);
   }
   else
      glColor3f(0.58f, 0.48f, 0.32f);

   glPushMatrix();
   glTranslated(x, 0.028, z);
   glRotated(rotation, 0, 1, 0);
   glBegin(GL_QUADS);
   glNormal3f(0, 1, 0);
   glTexCoord2f(0, 0);          glVertex3d(-length / 2, 0, -width / 2);
   glTexCoord2d(length / 3, 0); glVertex3d( length / 2, 0, -width / 2);
   glTexCoord2d(length / 3, 1); glVertex3d( length / 2, 0,  width / 2);
   glTexCoord2f(0, 1);          glVertex3d(-length / 2, 0,  width / 2);
   glEnd();
   glPopMatrix();
   glDisable(GL_TEXTURE_2D);
}

// Connect all eight scene zones with a central cross-road and short access
// branches. Roads sit at y=0.028, safely above the grass at y=-0.019.
void drawPathNetwork()
{
   drawRoadStrip(0, 0, 82, 3.2, 0);
   drawRoadStrip(0, 0, 82, 3.2, 90);

   drawRoadStrip(-22, -9, 18, 2.2, 90);  // Wind field access.
   drawRoadStrip( 18, -9, 18, 2.2, 90);  // Solar field access.
   drawRoadStrip( 25, -7, 14, 2.2, 0);   // Battery yard access.
   drawRoadStrip( 21, 13, 14, 2.2, 0);   // Substation access.
   drawRoadStrip( 20, 19.5, 13, 2.2, 90); // Control-building entrance access.
   drawRoadStrip( -5, 18, 14, 2.2, 90);  // Greenhouse access.
   drawRoadStrip(-18, 24, 14, 2.2, 0);   // Sheep paddock access.
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

// Place functional fencing around the battery yard, substation, and greenhouse.
// The sheep enclosure is owned by drawSheepPaddock() with the animated animals.
void drawFence()
{
   const FenceSection sections[] =
   {
      {29, -11, 10,  0}, {29, -3, 10, 0},
      {24,  -7,  8, 90}, {34, -7, 8, 90},
      {-5, 21, 12,  0}, {-5, 29, 12, 0},
      {-11, 25, 8, 90}, { 1, 25, 8, 90}
   };

   const int count = sizeof(sections) / sizeof(sections[0]);
   for (int i = 0; i < count; ++i)
   {
      drawFenceSection(sections[i].x, sections[i].z,
                       sections[i].length, sections[i].rotation);
   }
}

// Draw a small origin-centered rock used beside roads as erosion control.
void drawRock(double x, double z, double scale)
{
   glPushMatrix();
   glTranslated(x, 0.16 * scale, z);
   glRotated(17 * x + 11 * z, 0, 1, 0);
   glScaled(0.55 * scale, 0.32 * scale, 0.42 * scale);
   glColor3f(0.34f, 0.35f, 0.33f);
   drawBoxUnit();
   glPopMatrix();
}

// Draw a handmade zone sign with a wooden post and colored identification
// board. Sign colors correspond to energy, utility, or agricultural use.
void drawZoneSign(double x, double z, float r, float g, float b)
{
   glColor3f(0.38f, 0.22f, 0.10f);
   drawBox(x, 0.65, z, 0.16, 1.3, 0.16);
   glColor3f(r, g, b);
   drawBox(x, 1.25, z, 1.5, 0.58, 0.14);
   glColor3f(0.92f, 0.92f, 0.82f);
   drawBox(x, 1.25, z + 0.076, 1.12, 0.08, 0.025);
}

// Draw one reusable origin-centered distribution pole from a wooden post,
// metal crossarm, and three ceramic insulators.
void drawPowerPole()
{
   const float poleSpecular[] = {0.18f, 0.16f, 0.12f, 1.0f};
   const float metalSpecular[] = {0.42f, 0.44f, 0.46f, 1.0f};
   const float defaultSpecular[] = {0.22f, 0.22f, 0.22f, 1.0f};

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, poleSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 12.0f);
   glColor3f(0.34f, 0.23f, 0.12f);
   drawBox(0, 2.35, 0, 0.24, 4.70, 0.24);

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, metalSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 36.0f);
   glColor3f(0.42f, 0.44f, 0.45f);
   drawBox(0, 4.30, 0, 2.15, 0.16, 0.18);

   // White ceramic blocks distinguish the insulated cable attachment points.
   glColor3f(0.90f, 0.91f, 0.88f);
   for (int insulator = -1; insulator <= 1; ++insulator)
   {
      drawBox(0.72 * insulator, 4.48, 0, 0.16, 0.28, 0.16);
      drawBox(0.72 * insulator, 4.65, 0, 0.24, 0.07, 0.24);
   }

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);
}

// Draw one complete origin-centered electrical substation. The model contains
// a fenced concrete pad, transformer boxes, bus-support posts and beams,
// ceramic insulators, and a compact set of dark internal cables.
void drawSubstation()
{
   const float metalSpecular[] = {0.38f, 0.42f, 0.44f, 1.0f};
   const float defaultSpecular[] = {0.22f, 0.22f, 0.22f, 1.0f};

   glColor3f(0.38f, 0.39f, 0.40f);
   drawBox(0, 0.08, 0, 9.0, 0.16, 6.0);

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, metalSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 34.0f);
   glColor3f(0.26f, 0.31f, 0.34f);
   drawBox(-2.0, 0.75, 0.55, 1.8, 1.35, 1.4);
   drawBox( 0.4, 0.75, 0.55, 1.8, 1.35, 1.4);

   // Transformer cooling fins are raised slightly from the cabinets.
   glColor3f(0.15f, 0.18f, 0.19f);
   for (int transformer = 0; transformer < 2; ++transformer)
   {
      const double centerX = transformer ? 0.4 : -2.0;
      for (int fin = -2; fin <= 2; ++fin)
         drawBox(centerX + 0.25 * fin, 0.78, 1.27,
                 0.08, 0.92, 0.08);
   }

   // Two steel gantries carry a simple three-phase bus above the equipment.
   glColor3f(0.56f, 0.58f, 0.60f);
   for (int gantry = 0; gantry < 2; ++gantry)
   {
      const double z = -1.45 + 2.0 * gantry;
      drawBox(-3.35, 1.85, z, 0.16, 3.55, 0.16);
      drawBox( 3.35, 1.85, z, 0.16, 3.55, 0.16);
      drawBox(0, 3.52, z, 6.85, 0.16, 0.16);

      glColor3f(0.90f, 0.91f, 0.88f);
      for (int insulator = -1; insulator <= 1; ++insulator)
         drawBox(1.05 * insulator, 3.72, z,
                 0.18, 0.34, 0.18);
      glColor3f(0.56f, 0.58f, 0.60f);
   }

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);

   // The boundary belongs to the reusable substation rather than global fence
   // code, so inspection mode shows the same complete object as the farm.
   drawFenceSection(0, -3.1, 9.2, 0);
   drawFenceSection(0,  3.1, 9.2, 0);
   drawFenceSection(-4.6, 0, 6.2, 90);
   drawFenceSection( 4.6, 0, 6.2, 90);

   glDisable(GL_LIGHTING);
   glColor3f(0.06f, 0.07f, 0.08f);
   glLineWidth(2.0f);
   glBegin(GL_LINES);
   for (int phase = -1; phase <= 1; ++phase)
   {
      const double x = 1.05 * phase;
      glVertex3d(x, 3.90, -1.45);
      glVertex3d(x, 3.90, 0.55);
      glVertex3d(x, 3.90, 0.55);
      glVertex3d(0.4 + 0.25 * phase, 1.50, 0.90);
   }
   glEnd();
   glLineWidth(1.0f);
   if (lighting)
      glEnable(GL_LIGHTING);
}

// Draw the visual-only farm power network. A restrained branched layout links
// the wind, solar, battery, barn/control, and substation zones with repeated
// poles and three gently sagging cable phases per span.
void drawPowerLines()
{
   const double poles[][2] =
   {
      {-12.0, -10.0}, {-4.0, -5.0},   // Wind branch.
      { 10.0, -10.0}, { 3.0, -5.0},   // Solar branch.
      { 20.0,  -2.0}, {25.0,  6.0},   // Battery branch.
      { 17.0,  20.0}, {24.0, 17.5},   // Barn/control branch.
      { -0.5,  -4.0}, {12.0,  3.0},   // Central trunk.
      { 24.0,  10.0}                   // Substation entry.
   };
   const int spans[][2] =
   {
      {0, 1}, {1, 8}, {2, 3}, {3, 8},
      {8, 9}, {4, 5}, {5, 9}, {9, 10},
      {6, 7}, {7, 10}
   };

   const int poleCount = sizeof(poles) / sizeof(poles[0]);
   for (int pole = 0; pole < poleCount; ++pole)
   {
      glPushMatrix();
      glTranslated(poles[pole][0], 0, poles[pole][1]);
      drawPowerPole();
      glPopMatrix();
   }

   glDisable(GL_LIGHTING);
   glColor3f(0.055f, 0.06f, 0.065f);
   glLineWidth(1.6f);
   const int spanCount = sizeof(spans) / sizeof(spans[0]);
   for (int span = 0; span < spanCount; ++span)
   {
      const int start = spans[span][0];
      const int end = spans[span][1];
      for (int phase = -1; phase <= 1; ++phase)
      {
         glBegin(GL_LINE_STRIP);
         for (int segment = 0; segment <= 8; ++segment)
         {
            const double t = segment / 8.0;
            const double x = poles[start][0] +
               (poles[end][0] - poles[start][0]) * t + 0.72 * phase;
            const double z = poles[start][1] +
               (poles[end][1] - poles[start][1]) * t;
            const double sag = 0.34 * std::sin(180.0 * t * 3.1415927 / 180.0);
            glVertex3d(x, 4.65 - sag, z);
         }
         glEnd();
      }
   }
   glLineWidth(1.0f);
   if (lighting)
      glEnable(GL_LIGHTING);
}

// Add farm-purpose secondary details: zone signs and roadside erosion stones.
void drawFarmDetails()
{
   drawZoneSign(-22, -9.2, 0.34f, 0.62f, 0.82f);
   drawZoneSign( 18, -9.2, 0.18f, 0.34f, 0.62f);
   drawZoneSign( 24, -7.0, 0.82f, 0.64f, 0.12f);
   drawZoneSign( 21, 13.0, 0.78f, 0.32f, 0.18f);
   drawZoneSign( 20, 20.0, 0.62f, 0.24f, 0.18f);
   drawZoneSign( -5, 18.0, 0.24f, 0.62f, 0.42f);
   drawZoneSign(-18, 24.0, 0.78f, 0.78f, 0.68f);

   for (int i = -3; i <= 3; ++i)
   {
      drawRock(-7.0 + 5.0 * i, -2.1, 0.75 + 0.08 * (i & 1));
      drawRock( 2.1, -7.0 + 5.0 * i, 0.68 + 0.07 * ((i + 1) & 1));
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

// Draw the front-mounted facility sign. The board is opaque geometry, while
// the lettering is drawn unlit just in front of it so it remains readable.
void drawFarmSign()
{
   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, textureWood);
      glColor3f(0.76f, 0.58f, 0.30f);
   }
   else
      glColor3f(0.34f, 0.20f, 0.08f);

   drawBox(0, 1.78, -1.735, 2.35, 0.42, 0.10);
   glDisable(GL_TEXTURE_2D);

   const GLboolean lightingWasEnabled = glIsEnabled(GL_LIGHTING);
   glDisable(GL_LIGHTING);
   glColor3f(0.96f, 0.96f, 0.82f);
   glRasterPos3d(-0.92, 1.72, -1.795);
   const char* label = "CONTROL CENTER";
   for (const char* ch = label; *ch; ++ch)
      glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *ch);
   if (lightingWasEnabled)
      glEnable(GL_LIGHTING);
}

// Draw an origin-centered renewable-energy control building with textured
// walls and roof, metal details, windows, entrance, sign, and roof antenna.
void drawControlBuilding()
{
   const float wallSpecular[] = {0.08f, 0.07f, 0.06f, 1.0f};
   const float metalSpecular[] = {0.55f, 0.58f, 0.60f, 1.0f};
   const float defaultSpecular[] = {0.22f, 0.22f, 0.22f, 1.0f};

   // Painted wall surfaces remain mostly diffuse.
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, wallSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 8.0f);

   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, textureWood);
      glColor3f(0.74f, 0.82f, 0.76f);
   }
   else
      glColor3f(0.38f, 0.50f, 0.42f);
   glPushMatrix();
   glTranslated(0, 1.1, 0);
   glScaled(4.6, 2.2, 3.4);
   drawBoxUnit(5, 2, 4);
   glPopMatrix();

   if (textures)
   {
      glBindTexture(GL_TEXTURE_2D, textureRoof);
      glColor3f(0.90f, 0.90f, 0.88f);
   }
   else
      glColor3f(0.22f, 0.25f, 0.26f);
   glPushMatrix();
   glTranslated(0, 2.15, 0);
   glScaled(5.0, 1.35, 3.8);
   drawGableRoofUnit();
   glPopMatrix();
   glDisable(GL_TEXTURE_2D);

   // Front entrance uses a wood texture and metal frame/handle.
   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, textureWood);
      glColor3f(0.70f, 0.46f, 0.24f);
   }
   else
      glColor3f(0.38f, 0.20f, 0.08f);
   drawBox(0, 0.82, -1.725, 1.05, 1.55, 0.10);
   glDisable(GL_TEXTURE_2D);

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, metalSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 48.0f);
   glColor3f(0.68f, 0.72f, 0.72f);
   drawBox(-0.56, 0.82, -1.79, 0.07, 1.65, 0.07);
   drawBox( 0.56, 0.82, -1.79, 0.07, 1.65, 0.07);
   drawBox(0, 1.62, -1.79, 1.18, 0.07, 0.07);
   glColor3f(0.88f, 0.70f, 0.18f);
   drawBox(0.37, 0.80, -1.81, 0.08, 0.08, 0.06);

   // Slightly raised blue-gray panes avoid z-fighting with the wall.
   glColor3f(0.28f, 0.58f, 0.70f);
   drawBox(-1.42, 1.12, -1.725, 0.85, 0.72, 0.08);
   drawBox( 1.42, 1.12, -1.725, 0.85, 0.72, 0.08);
   drawBox(-2.325, 1.12, 0.78, 0.08, 0.72, 1.05);
   drawBox( 2.325, 1.12, 0.78, 0.08, 0.72, 1.05);

   drawFarmSign();

   // Rooftop equipment identifies the building as facility infrastructure.
   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, textureMetal);
   }
   glColor3f(0.52f, 0.56f, 0.58f);
   drawBox(-1.25, 2.92, 0.25, 0.72, 0.42, 0.62);
   drawBox( 1.15, 3.05, 0.15, 0.10, 1.15, 0.10);
   glDisable(GL_TEXTURE_2D);

   glDisable(GL_LIGHTING);
   glColor3f(0.80f, 0.84f, 0.86f);
   glLineWidth(3.0f);
   glBegin(GL_LINES);
   glVertex3d(1.15, 3.55, 0.15);
   glVertex3d(0.75, 3.28, 0.15);
   glVertex3d(1.15, 3.55, 0.15);
   glVertex3d(1.55, 3.28, 0.15);
   glEnd();
   glLineWidth(1.0f);
   if (lighting)
      glEnable(GL_LIGHTING);

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);
}

// Draw one raised soil bed with a repeated row of simple leafy plants.
// The row is origin-centered so drawGreenhouse() can place multiple rows.
void drawPlantRow()
{
   glDisable(GL_TEXTURE_2D);

   glColor3f(0.30f, 0.16f, 0.07f);
   drawBox(0, 0.13, 0, 3.25, 0.24, 0.42);

   for (int plant = -3; plant <= 3; ++plant)
   {
      const double x = plant * 0.46;

      glColor3f(0.18f, 0.48f, 0.16f);
      drawBox(x, 0.39, 0, 0.055, 0.48, 0.055);

      glColor3f(0.28f, 0.68f, 0.22f);
      glPushMatrix();
      glTranslated(x - 0.09, 0.48, 0);
      glScaled(0.17, 0.10, 0.08);
      glutSolidSphere(1.0, 10, 8);
      glPopMatrix();

      glPushMatrix();
      glTranslated(x + 0.09, 0.58, 0);
      glScaled(0.17, 0.10, 0.08);
      glutSolidSphere(1.0, 10, 8);
      glPopMatrix();
   }
}

// Draw the greenhouse base, door, vertical posts, rails, ridge, and rafters.
// All greenhouse helpers are centered around the origin; world placement is
// applied only by the full-scene caller.
void drawGreenhouseFrame()
{
   const double roofAngle = 32.735;
   const double roofLength = std::sqrt(1.4 * 1.4 + 0.9 * 0.9);
   const double frameX[] = {-2.0, 0.0, 2.0};

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

   // Front door frame and handle make the entrance readable through the glass.
   drawBox(-0.53, 0.78, -1.43, 0.08, 1.42, 0.08);
   drawBox( 0.53, 0.78, -1.43, 0.08, 1.42, 0.08);
   drawBox(0, 1.46, -1.43, 1.14, 0.08, 0.08);
   glColor3f(0.78f, 0.63f, 0.18f);
   drawBox(0.36, 0.75, -1.49, 0.07, 0.07, 0.07);
   glColor3f(0.62f, 0.68f, 0.64f);

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
   glColor4f(0.48f, 0.76f, 0.78f, 0.38f);

   // Back wall uses two broad panes.
   drawBox(-1.0, 0.82, 1.405, 1.82, 1.32, 0.035);
   drawBox( 1.0, 0.82, 1.405, 1.82, 1.32, 0.035);

   // The front wall leaves a centered opening occupied by a glass door.
   drawBox(-1.35, 0.82, -1.405, 1.12, 1.32, 0.035);
   drawBox( 1.35, 0.82, -1.405, 1.12, 1.32, 0.035);
   glColor4f(0.40f, 0.70f, 0.74f, 0.46f);
   drawBox(0, 0.77, -1.445, 0.96, 1.34, 0.035);
   glColor4f(0.48f, 0.76f, 0.78f, 0.38f);

   for (int side = -1; side <= 1; side += 2)
   {
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

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);
}

// Draw the origin-centered greenhouse in one of two explicit render passes.
// Pass 1 (glassPass=false) draws only opaque frame, door hardware, soil, and
// plants. Pass 2 is called after all opaque scene geometry and draws only glass.
// Depth testing stays enabled in pass 2, but depth writes are temporarily
// disabled so transparent panes do not conceal plants or scenery behind them.
void drawGreenhouse(bool glassPass = false)
{
   if (!glassPass)
   {
      drawGreenhouseFrame();

      glPushMatrix();
      glTranslated(0, 0, -0.55);
      drawPlantRow();
      glPopMatrix();

      glPushMatrix();
      glTranslated(0, 0, 0.55);
      drawPlantRow();
      glPopMatrix();
      return;
   }

   if (!glassVisible)
      return;

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDepthMask(GL_FALSE);
   drawGreenhouseGlassPanels();
   glDepthMask(GL_TRUE);
   glDisable(GL_BLEND);
}

// Draw one complete origin-centered solar-panel assembly. All components use
// the handmade quad-based box primitive: concrete base, steel stand and braces,
// tilted photovoltaic face, raised frame, and raised cell-divider strips.
void drawSolarPanel()
{
   const float panelSpecular[] = {0.58f, 0.68f, 0.82f, 1.0f};
   const float frameSpecular[] = {0.40f, 0.42f, 0.46f, 1.0f};
   const float defaultSpecular[] = {0.22f, 0.22f, 0.22f, 1.0f};

   glDisable(GL_TEXTURE_2D);

   // A broad concrete foot keeps the stand visibly anchored to the terrain.
   glColor3f(0.32f, 0.34f, 0.35f);
   drawBox(0, 0.07, 0, 1.35, 0.14, 0.72);

   // Steel support and diagonal braces use a moderate metallic highlight.
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, frameSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 36.0f);
   glColor3f(0.42f, 0.44f, 0.47f);
   drawBox(-0.68, 0.54, 0, 0.10, 0.94, 0.10);
   drawBox( 0.68, 0.54, 0, 0.10, 0.94, 0.10);

   glPushMatrix();
   glTranslated(-0.68, 0.55, 0.25);
   glRotated(-24, 1, 0, 0);
   drawBox(0, 0, 0, 0.09, 0.09, 0.78);
   glPopMatrix();

   glPushMatrix();
   glTranslated(0.68, 0.55, 0.25);
   glRotated(-24, 1, 0, 0);
   drawBox(0, 0, 0, 0.09, 0.09, 0.78);
   glPopMatrix();

   glPushMatrix();
   glTranslated(0, 1.08, 0);
   glRotated(-24, 1, 0, 0);

   // The dark blue face has high shininess so the moving positional light
   // creates a readable reflective highlight across the solar field.
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, panelSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 72.0f);
   glColor3f(0.04f, 0.12f, 0.24f);
   drawBox(0, 0, 0, 2.18, 0.055, 1.18);

   // Frame and divider strips sit above the panel face, avoiding coplanar
   // surfaces and the depth-buffer flicker caused by z-fighting.
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, frameSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 44.0f);
   glColor3f(0.54f, 0.58f, 0.62f);
   drawBox(0, 0.055, -0.65, 2.40, 0.10, 0.10);
   drawBox(0, 0.055,  0.65, 2.40, 0.10, 0.10);
   drawBox(-1.15, 0.055, 0, 0.10, 0.10, 1.20);
   drawBox( 1.15, 0.055, 0, 0.10, 0.10, 1.20);

   glColor3f(0.20f, 0.38f, 0.58f);
   for (int i = -1; i <= 1; ++i)
      drawBox(0.52 * i, 0.061, 0, 0.025, 0.018, 1.16);
   drawBox(0, 0.061, 0, 2.16, 0.018, 0.025);

   glPopMatrix();

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);
}

// Draw rows of repeated solar panels using transforms around one shared model.
void drawSolarFarm()
{
   glPushMatrix();
   glTranslated(solarZoneX, 0, solarZoneZ);
   glRotated(-12, 0, 1, 0);

   for (int row = 0; row < 3; ++row)
   {
      for (int column = 0; column < 3; ++column)
      {
         glPushMatrix();
         glTranslated((column - 1) * 3.0, 0, (row - 1) * 2.35);
         drawSolarPanel();
         glPopMatrix();
      }
   }

   glPopMatrix();
}

// Draw one origin-centered industrial battery container with a concrete
// plinth, textured metal enclosure, front doors, vents, warning label, status
// lamps, cable housing, and a side electrical box.
void drawBatteryUnit()
{
   const float cabinetSpecular[] = {0.32f, 0.34f, 0.36f, 1.0f};
   const float defaultSpecular[] = {0.22f, 0.22f, 0.22f, 1.0f};

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, cabinetSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 36.0f);

   glColor3f(0.42f, 0.43f, 0.44f);
   drawBox(0, 0.10, 0, 1.45, 0.20, 1.05);

   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, textureMetal);
   }
   glColor3f(0.76f, 0.80f, 0.78f);
   drawBox(0, 1.05, 0, 1.25, 1.75, 0.88);

   glColor3f(0.23f, 0.27f, 0.28f);
   drawBox(-0.31, 1.05, 0.451, 0.56, 1.55, 0.035);
   drawBox( 0.31, 1.05, 0.451, 0.56, 1.55, 0.035);
   drawBox(0, 1.97, 0, 0.72, 0.16, 0.52);
   glDisable(GL_TEXTURE_2D);

   // Raised door seams and handles remain clear at inspection distance.
   glColor3f(0.08f, 0.09f, 0.10f);
   drawBox(0, 1.05, 0.473, 0.025, 1.50, 0.018);
   drawBox(-0.08, 1.12, 0.486, 0.025, 0.22, 0.025);
   drawBox( 0.08, 1.12, 0.486, 0.025, 0.22, 0.025);

   // Thin raised slats provide readable cooling vents without external models.
   glColor3f(0.08f, 0.10f, 0.11f);
   for (int vent = 0; vent < 5; ++vent)
   {
      const double y = 0.58 + 0.13 * vent;
      drawBox(-0.31, y, 0.474, 0.38, 0.035, 0.025);
      drawBox( 0.31, y, 0.474, 0.38, 0.035, 0.025);
   }

   glColor3f(0.88f, 0.72f, 0.12f);
   drawBox(-0.08, 1.55, 0.476, 0.08, 0.08, 0.025);
   glColor3f(0.16f, 0.82f, 0.28f);
   drawBox( 0.08, 1.55, 0.476, 0.08, 0.08, 0.025);

   // A raised yellow warning plate and black lightning mark use only handmade
   // polygons and sit forward of the doors to avoid z-fighting.
   glColor3f(0.92f, 0.72f, 0.08f);
   glBegin(GL_TRIANGLES);
   glNormal3f(0, 0, 1);
   glVertex3d(-0.16, 1.78, 0.490);
   glVertex3d( 0.16, 1.78, 0.490);
   glVertex3d( 0.00, 2.05, 0.490);
   glEnd();
   glColor3f(0.08f, 0.08f, 0.07f);
   glBegin(GL_POLYGON);
   glNormal3f(0, 0, 1);
   glVertex3d( 0.02, 1.99, 0.493);
   glVertex3d(-0.05, 1.87, 0.493);
   glVertex3d( 0.01, 1.87, 0.493);
   glVertex3d(-0.02, 1.80, 0.493);
   glVertex3d( 0.08, 1.91, 0.493);
   glVertex3d( 0.02, 1.91, 0.493);
   glEnd();

   // Side-mounted cable box and short conduit identify the electrical output.
   glColor3f(0.28f, 0.31f, 0.32f);
   drawBox(0.69, 0.82, 0.05, 0.16, 0.62, 0.48);
   glColor3f(0.06f, 0.07f, 0.08f);
   drawBox(0.78, 0.42, 0.05, 0.08, 0.38, 0.12);

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);
}

// Place four repeated battery containers on a concrete pad inside the existing
// fenced battery zone. Handmade collection boxes and raised cable conduits
// visually route their output north toward the substation.
void drawBatteryYard()
{
   glColor3f(0.34f, 0.35f, 0.36f);
   drawBox(batteryZoneX, 0.06, batteryZoneZ, 8.8, 0.12, 6.6);

   glPushMatrix();
   glTranslated(batteryZoneX, 0.12, batteryZoneZ);
   for (int row = 0; row < 2; ++row)
   {
      for (int column = 0; column < 2; ++column)
      {
         glPushMatrix();
         glTranslated((column ? 1.9 : -1.9), 0,
                      (row ? 1.45 : -1.45));
         drawBatteryUnit();
         glPopMatrix();
      }
   }
   glPopMatrix();

   // Collection boxes receive short branch conduits from both cabinet rows.
   glColor3f(0.18f, 0.21f, 0.22f);
   drawBox(batteryZoneX + 3.55, 0.48, batteryZoneZ - 1.45,
           0.48, 0.84, 0.62);
   drawBox(batteryZoneX + 3.55, 0.48, batteryZoneZ + 1.45,
           0.48, 0.84, 0.62);
   drawBox(batteryZoneX + 2.75, 0.20, batteryZoneZ - 1.45,
           1.35, 0.12, 0.16);
   drawBox(batteryZoneX + 2.75, 0.20, batteryZoneZ + 1.45,
           1.35, 0.12, 0.16);

   // The main conduit exits the battery fence and runs to the south edge of
   // the substation fence, providing a clear visual connection between zones.
   glColor3f(0.10f, 0.12f, 0.13f);
   drawBox(batteryZoneX + 3.55, 0.19, batteryZoneZ,
           0.18, 0.14, 2.55);
   drawBox(batteryZoneX + 3.55, 0.19, 3.0,
           0.18, 0.14, 17.2);
   drawBox(substationZoneX + 2.25, 0.19, 11.45,
           4.60, 0.14, 0.18);
}

// Draw an origin-centered weather station. Its anemometer uses the shared
// elapsed-time angle advanced from windSpeed in idle(), so the visible sensor
// responds immediately to the same wind value used by turbines and the HUD.
void drawWeatherStation()
{
   const float metalSpecular[] = {0.42f, 0.45f, 0.48f, 1.0f};
   const float defaultSpecular[] = {0.22f, 0.22f, 0.22f, 1.0f};

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, metalSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 48.0f);

   if (textures)
   {
      glEnable(GL_TEXTURE_2D);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, textureMetal);
   }
   glColor3f(0.48f, 0.52f, 0.54f);
   drawBox(0, 1.7, 0, 0.16, 3.4, 0.16);
   drawBox(0, 0.08, 0, 0.72, 0.16, 0.72);
   glDisable(GL_TEXTURE_2D);

   // Weatherproof sensor enclosure and small status lamp.
   glColor3f(0.78f, 0.80f, 0.76f);
   drawBox(0, 1.05, 0, 0.86, 0.62, 0.56);
   glColor3f(0.10f, 0.16f, 0.18f);
   drawBox(0, 1.05, -0.291, 0.58, 0.30, 0.025);
   glColor3f(0.18f, 0.82f, 0.30f);
   drawBox(0.27, 1.22, -0.31, 0.07, 0.07, 0.04);

   // Tiny solar panel powers the remote instrument box.
   glPushMatrix();
   glTranslated(-0.62, 2.15, 0);
   glRotated(-28, 0, 0, 1);
   glColor3f(0.08f, 0.20f, 0.38f);
   drawBox(0, 0, 0, 0.82, 0.06, 0.58);
   glColor3f(0.58f, 0.62f, 0.64f);
   drawBox(0, -0.08, 0, 0.92, 0.08, 0.68);
   glPopMatrix();

   // Wind vane rotates as a single horizontal assembly around the mast.
   glPushMatrix();
   glTranslated(0, 3.08, 0);
   glRotated(-32, 0, 1, 0);
   glColor3f(0.70f, 0.74f, 0.76f);
   drawBox(0, 0, 0, 2.0, 0.09, 0.09);
   glColor3f(0.82f, 0.30f, 0.14f);
   drawBox(0.72, 0.12, 0, 0.48, 0.32, 0.08);
   glBegin(GL_TRIANGLES);
   glNormal3f(0, 0, -1);
   glVertex3d(-1.22, 0, -0.05);
   glVertex3d(-0.88, 0.20, -0.05);
   glVertex3d(-0.88, -0.20, -0.05);
   glNormal3f(0, 0, 1);
   glVertex3d(-1.22, 0, 0.05);
   glVertex3d(-0.88, -0.20, 0.05);
   glVertex3d(-0.88, 0.20, 0.05);
   glEnd();
   glPopMatrix();

   // Rotate all three cup arms together. No physics model is needed: angular
   // speed is a linear multiple of windSpeed and is integrated in idle().
   glPushMatrix();
   glTranslated(0, 3.62, 0);
   glRotated(anemometerAngle, 0, 1, 0);
   glColor3f(0.72f, 0.76f, 0.78f);
   for (int cup = 0; cup < 3; ++cup)
   {
      glPushMatrix();
      glRotated(120 * cup, 0, 1, 0);
      drawBox(0.58, 0, 0, 1.12, 0.07, 0.07);
      glColor3f(0.30f, 0.34f, 0.36f);
      drawBox(1.13, 0, 0.14, 0.24, 0.30, 0.28);
      glColor3f(0.72f, 0.76f, 0.78f);
      glPopMatrix();
   }
   drawBox(0, 0, 0, 0.20, 0.20, 0.20);
   glPopMatrix();

   drawBox(0, 3.38, 0, 0.10, 0.48, 0.10);

   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24.0f);
}

// Draw one origin-centered low-poly sheep using only transformed handmade
// boxes. The broad white body, dark face, ears, and four legs stay readable
// Emit one vertex and its ellipsoid normal for the handmade low-poly body.
void drawEllipsoidVertex(double latitude, double longitude,
                         double radiusX, double radiusY, double radiusZ)
{
   const double x = radiusX * Cos(latitude) * Cos(longitude);
   const double y = radiusY * Sin(latitude);
   const double z = radiusZ * Cos(latitude) * Sin(longitude);
   double nx = x / (radiusX * radiusX);
   double ny = y / (radiusY * radiusY);
   double nz = z / (radiusZ * radiusZ);
   const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
   nx /= length;
   ny /= length;
   nz /= length;
   glNormal3d(nx, ny, nz);
   glVertex3d(x, y, z);
}

// Draw a deliberately low-poly handmade ellipsoid without imported geometry.
void drawLowPolyEllipsoid(double radiusX, double radiusY, double radiusZ)
{
   const int slices = 10;
   const int stacks = 6;
   for (int stack = 0; stack < stacks; ++stack)
   {
      const double latitude0 = -90.0 + 180.0 * stack / stacks;
      const double latitude1 = -90.0 + 180.0 * (stack + 1) / stacks;
      glBegin(GL_QUAD_STRIP);
      for (int slice = 0; slice <= slices; ++slice)
      {
         const double longitude = 360.0 * slice / slices;
         drawEllipsoidVertex(latitude0, longitude,
                             radiusX, radiusY, radiusZ);
         drawEllipsoidVertex(latitude1, longitude,
                             radiusX, radiusY, radiusZ);
      }
      glEnd();
   }
}

// Draw one origin-centered low-poly sheep. The optional phase offsets the
// simple periodic gait so paddock animals do not move in lockstep.
void drawSheep(double phase = 0.0)
{
   const double timeSeconds = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
   const double legSwing = 14.0 * std::sin(2.8 * timeSeconds + phase);
   const double headBob = 0.035 * std::sin(2.8 * timeSeconds + phase);

   glColor3f(0.90f, 0.88f, 0.80f);
   glPushMatrix();
   glTranslated(0, 1.02, 0);
   drawLowPolyEllipsoid(0.88, 0.52, 0.55);
   glPopMatrix();

   // Small repeated wool bumps keep the silhouette readable but intentionally
   // stylized rather than anatomically realistic.
   for (int bump = -2; bump <= 2; ++bump)
   {
      glPushMatrix();
      glTranslated(0.31 * bump, 1.43 - 0.025 * std::abs(bump), -0.02);
      drawLowPolyEllipsoid(0.25, 0.19, 0.26);
      glPopMatrix();
   }

   glPushMatrix();
   glTranslated(0, headBob, 0);
   glColor3f(0.24f, 0.22f, 0.20f);
   drawBox(0, 1.12, 0.63, 0.58, 0.66, 0.46);
   drawBox(-0.42, 1.28, 0.65, 0.30, 0.11, 0.18);
   drawBox( 0.42, 1.28, 0.65, 0.30, 0.11, 0.18);
   glColor3f(0.08f, 0.07f, 0.06f);
   drawBox(-0.15, 1.20, 0.875, 0.06, 0.06, 0.025);
   drawBox( 0.15, 1.20, 0.875, 0.06, 0.06, 0.025);
   glPopMatrix();

   glColor3f(0.22f, 0.20f, 0.18f);
   for (int side = -1; side <= 1; side += 2)
   {
      for (int end = -1; end <= 1; end += 2)
      {
         glPushMatrix();
         glTranslated(0.55 * side, 0.72, 0.29 * end);
         glRotated(end * legSwing, 1, 0, 0);
         drawBox(0, -0.34, 0, 0.17, 0.68, 0.17);
         glPopMatrix();
      }
   }

   glColor3f(0.82f, 0.80f, 0.72f);
   glPushMatrix();
   glTranslated(0, 1.15, -0.60);
   glRotated(-28, 1, 0, 0);
   drawLowPolyEllipsoid(0.18, 0.16, 0.24);
   glPopMatrix();
}

// Draw the fenced paddock and five sheep following fixed elliptical paths.
// This is deterministic path animation only; there is no collision or AI.
void drawSheepPaddock()
{
   glPushMatrix();
   glTranslated(paddockZoneX, 0, paddockZoneZ);

   drawFenceSection(0, -7, 16, 0);
   drawFenceSection(0,  7, 16, 0);
   drawFenceSection(-8, 0, 14, 90);
   drawFenceSection(8, 3.5, 7, 90);
   drawFenceSection(8, -4.5, 5, 90);

   // Water trough remains stationary near the fence and outside walking paths.
   glColor3f(0.25f, 0.34f, 0.38f);
   drawBox(-6.1, 0.28, 4.9, 2.6, 0.45, 0.85);
   glColor3f(0.20f, 0.46f, 0.68f);
   drawBox(-6.1, 0.52, 4.9, 2.25, 0.08, 0.58);

   const double timeSeconds = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
   for (int sheep = 0; sheep < 5; ++sheep)
   {
      const double phase = 72.0 * sheep;
      const double angle = 12.0 * timeSeconds + phase;
      const double radiusX = 4.7 - 0.35 * (sheep % 2);
      const double radiusZ = 4.2 - 0.40 * (sheep % 3);
      const double x = radiusX * Cos(angle);
      const double z = radiusZ * Sin(angle);
      const double dx = -radiusX * Sin(angle);
      const double dz =  radiusZ * Cos(angle);
      const double heading =
         std::atan2(dx, dz) * 180.0 / 3.1415927;

      glPushMatrix();
      glTranslated(x, 0, z);
      glRotated(heading, 0, 1, 0);
      glScaled(0.82, 0.82, 0.82);
      drawSheep(phase * 3.1415927 / 180.0);
      glPopMatrix();
   }

   glPopMatrix();
}

// Draw one origin-centered farmer with a hat, head, torso, arms, and legs.
void drawFarmer()
{
   glColor3f(0.78f, 0.58f, 0.42f);
   drawBox(0, 2.35, 0, 0.58, 0.58, 0.52);
   glColor3f(0.72f, 0.52f, 0.18f);
   drawBox(0, 2.70, 0, 1.05, 0.12, 0.78);
   drawBox(0, 2.86, 0, 0.62, 0.28, 0.55);

   glColor3f(0.22f, 0.42f, 0.66f);
   drawBox(0, 1.55, 0, 0.78, 1.05, 0.46);
   drawBox(-0.58, 1.58, 0, 0.22, 1.05, 0.22);
   drawBox( 0.58, 1.58, 0, 0.22, 1.05, 0.22);
   glColor3f(0.20f, 0.24f, 0.28f);
   drawBox(-0.22, 0.55, 0, 0.27, 1.10, 0.30);
   drawBox( 0.22, 0.55, 0, 0.27, 1.10, 0.30);
}

// Place the weather station and farm figures in their functional full-scene
// locations. Inspection mode draws the same models directly at the origin.
void drawFarmCharactersAndInstruments()
{
   glPushMatrix();
   // Sited beside the turbine access road for representative wind readings.
   glTranslated(-15.0, 0, -9.0);
   drawWeatherStation();
   glPopMatrix();

   glPushMatrix();
   glTranslated(-20.0, 0, 21.0);
   glRotated(-25, 0, 1, 0);
   drawFarmer();
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
   // Tree lines act as windbreaks near buildings and paddock boundaries.
   const double treePositions[][3] =
   {
      {-39, -28, 1.2}, {-39, -18, 1.0}, {-39, -8, 1.1},
      {-38,  10, 1.0}, {-38,  20, 1.2}, {-38, 32, 1.0},
      { -9,  36, 1.1}, {  2,  36, 1.0}, { 13, 36, 1.2},
      { 28,  36, 1.1}, { 39,  28, 1.0}, { 39,  5, 1.1},
      { 39, -18, 1.0}, { 28, -39, 1.1}, {  8, -39, 1.0}
   };

   const int count = sizeof(treePositions) / sizeof(treePositions[0]);
   for (int i = 0; i < count; ++i)
      drawTree(treePositions[i][0], treePositions[i][1],
               treePositions[i][2]);
}

// Draw the wind-turbine field by transforming repeated instances of the same
// origin-centered geometry.
void drawTurbineField()
{
   const Instance windmills[] =
   {
      {-1.0, 0.0,  0.5, 1.35, 1.35, 1.35,  10, 0.72f, 0.72f, 0.68f,  12},
      {-5.1, 0.0, -2.3, 0.82, 0.95, 0.82, -28, 0.48f, 0.62f, 0.70f,  48},
      { 4.2, 0.0, -3.8, 0.62, 0.72, 0.62,  37, 0.72f, 0.58f, 0.42f, -25}
   };

   const int count = sizeof(windmills) / sizeof(windmills[0]);
   glPushMatrix();
   glTranslated(windZoneX + 0.6, 0, windZoneZ + 1.9);
   for (int i = 0; i < count; ++i)
      drawWindmillInstance(windmills[i]);
   glPopMatrix();
}

// Place the control building beside its north-south access road. That branch
// meets the substation road at (20, 13), linking operations to grid equipment.
void drawBarnGroup()
{
   glPushMatrix();
   glTranslated(barnZoneX, 0, barnZoneZ);
   glRotated(-18, 0, 1, 0);
   drawControlBuilding();
   glPopMatrix();
}

// Draw only the opaque greenhouse base, posts, rails, ridge, and rafters.
void drawGreenhouseOpaque()
{
   glPushMatrix();
   glTranslated(greenhouseZoneX, 0, greenhouseZoneZ);
   drawGreenhouse();
   glPopMatrix();
}

// Draw transparent greenhouse glass after every opaque object. The blending
// and depth-write state is localized inside drawGreenhouse(true).
void drawGreenhouseTransparent()
{
   glPushMatrix();
   glTranslated(greenhouseZoneX, 0, greenhouseZoneZ);
   drawGreenhouse(true);
   glPopMatrix();
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

   glPushMatrix();
   glTranslated(windZoneX, 0, windZoneZ);
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
   glPopMatrix();

   // Never allow the ribbon shader to leak into glass, HUD, or later frames.
   glUseProgram(0);

   glDepthMask(GL_TRUE);
   glDisable(GL_BLEND);
}

// -----------------------------------------------------------------------------
// Scene dispatch
// -----------------------------------------------------------------------------

// Draw environmental and secondary objects used around the energy farm.
void drawSecondaryObjects()
{
   drawExpandedTerrain();
   drawPathNetwork();
   drawFence();
   drawTreeGroup();
   drawFarmDetails();
}

// Draw every opaque group at its zone anchor. The southern half contains the
// wind, solar, and battery zones; the northern half contains utilities,
// buildings, greenhouse, and livestock support.
void drawScene()
{
   drawSecondaryObjects();
   drawFarmCharactersAndInstruments();
   drawSheepPaddock();
   drawBarnGroup();
   drawTurbineField();
   drawGreenhouseOpaque();
   drawSolarFarm();
   drawBatteryYard();
   glPushMatrix();
   glTranslated(substationZoneX, 0, substationZoneZ);
   drawSubstation();
   glPopMatrix();
   drawPowerLines();
}

// Draw a neutral inspection floor and one-unit grid at y=0. It is intentionally
// untextured so object textures, materials, silhouette, and scale stay clear.
void drawInspectionGround()
{
   glDisable(GL_TEXTURE_2D);
   glColor3f(0.24f, 0.26f, 0.27f);
   drawBox(0, -0.075, 0, 12.0, 0.15, 12.0);

   glDisable(GL_LIGHTING);
   glColor3f(0.42f, 0.44f, 0.45f);
   glBegin(GL_LINES);
   for (int line = -6; line <= 6; ++line)
   {
      glVertex3d(line, 0.005, -6);
      glVertex3d(line, 0.005,  6);
      glVertex3d(-6, 0.005, line);
      glVertex3d( 6, 0.005, line);
   }
   glEnd();
   if (lighting)
      glEnable(GL_LIGHTING);
}

// Draw exactly one major object at the origin for professor-facing inspection.
// Existing geometry is reused with inverse translations where legacy drawing
// functions still contain their original local placement transform.
void drawInspectionObject()
{
   drawInspectionGround();

   switch (inspectionMode)
   {
      case 1:
         glPushMatrix();
         glScaled(1.35, 1.35, 1.35);
         drawTurbine();
         glPopMatrix();
         break;
      case 2:
         drawSolarPanel();
         break;
      case 3:
         drawBatteryUnit();
         break;
      case 4:
         drawSubstation();
         break;
      case 5:
         drawGreenhouse();
         break;
      case 6:
         // Face the south-side entrance toward the default inspection camera.
         glPushMatrix();
         glRotated(180, 0, 1, 0);
         drawControlBuilding();
         glPopMatrix();
         break;
      case 7:
         drawWeatherStation();
         break;
      case 8:
         glPushMatrix();
         glScaled(1.5, 1.5, 1.5);
         drawSheep();
         glPopMatrix();
         break;
      case 9:
         drawFarmer();
         break;
   }
}

// Draw transparent scene content after opaque geometry. Greenhouse inspection
// uses origin-centered glass while full-scene mode retains world placement.
void drawTransparentPass()
{
   if (inspectionMode == 0)
   {
      drawWindRibbons();
      drawGreenhouseTransparent();
   }
   else if (inspectionMode == 5 && glassVisible)
      drawGreenhouse(true);
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
   if (inspectionMode != 0)
      glDisable(GL_FOG);

   const double lightRadius = inspectionMode == 0 ? 38.0 : 7.0;
   const double activeLightHeight =
      inspectionMode == 0 ? lightHeight : 7.0;
   const float lightPosition[] =
   {
      static_cast<float>(lightRadius * Cos(lightAngle)),
      static_cast<float>(activeLightHeight),
      static_cast<float>(lightRadius * Sin(lightAngle)),
      1.0f
   };
   if (inspectionMode == 0)
      drawLightMarker(lightPosition[0], lightPosition[1], lightPosition[2]);
   if (lighting)
      ConfigureLighting(lightPosition);
   else
      glDisable(GL_LIGHTING);

   // Mode zero renders the complete expanded farm. Modes one through nine
   // render exactly one origin-centered object over the neutral inspection grid.
   if (inspectionMode == 0)
      drawScene();
   else
      drawInspectionObject();
   glDisable(GL_LIGHTING);
   if (axes)
      DrawAxes();

   if (lighting)
      glEnable(GL_LIGHTING);

   // Transparent glass is drawn after all opaque scene geometry so the
   // existing depth buffer can reject hidden fragments without glass writing
   // new depth values that would incorrectly conceal objects behind it.
   drawTransparentPass();

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
   const double anemometerRpm =
      anemometerDegreesPerSecond * windSpeed / 6.0;
   char windText[140];
   std::snprintf(windText, sizeof(windText),
                 "Wind Speed: %.2f   Turbine RPM: %.2f   Anemometer RPM: %.2f   Blades: %s",
                 windSpeed, bladeRpm, anemometerRpm,
                 rotateBlades ? "running" : "paused");

   DrawText(10, 170, "Shader-Based Renewable Energy Farm Visualization");
   DrawText(10, 150, stateText);
   DrawText(10, 130, windText);
   DrawText(10, 110, lightText);
   DrawText(10, 90, viewText);
   DrawText(10, 70, ModeName());
   DrawText(10, 50, "0-9: inspect  arrows: navigate  l: light  f: fog  t: textures  g: glass  S: wind flow  [ / ]: wind");
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

// Apply a centered camera preset for full scene or one origin-centered object.
// Arrow keys, camera-mode cycling, zoom, and first-person controls remain
// unchanged after the preset is selected.
void SetInspectionMode(int selectedMode)
{
   inspectionMode = selectedMode;
   mode = 1;

   if (inspectionMode == 0)
   {
      viewTargetX = 0;
      viewTargetY = 1;
      viewTargetZ = 0;
      th = 35;
      ph = 30;
      dim = 42;
      return;
   }

   viewTargetX = 0;
   viewTargetZ = 0;
   th = 30;
   ph = 16;

   switch (inspectionMode)
   {
      case 1: viewTargetY = 2.2; dim = 5.0; break;
      case 2: viewTargetY = 0.8; dim = 3.0; break;
      case 3: viewTargetY = 1.0; dim = 2.8; break;
      case 4: viewTargetY = 1.4; dim = 4.0; break;
      case 5: viewTargetY = 1.2; dim = 4.2; break;
      case 6: viewTargetY = 1.2; dim = 3.6; break;
      case 7: viewTargetY = 1.8; dim = 4.0; break;
      case 8: viewTargetY = 0.9; dim = 3.0; break;
      case 9: viewTargetY = 1.4; dim = 3.4; break;
   }
}

// Restore the original full-scene camera and moving-light position.
void ResetCamera()
{
   SetInspectionMode(0);
   fov = 60;
   fpX = 0;
   fpY = 1;
   fpZ = 42;
   fpYaw = 0;
   lightAngle = 90;
   lightHeight = 25;
}

// Adjust orthographic size or perspective field of view.
void AdjustZoom(int direction)
{
   if (mode == 0)
   {
      dim -= 0.5 * direction;
      if (dim < 3)
         dim = 3;
      if (dim > 80)
         dim = 80;
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
      if (lightHeight > 60)
         lightHeight = 60;
   }
   else if (ch >= '0' && ch <= '9')
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
   // The weather station is an independent sensor: it follows windSpeed even
   // when turbine blade animation is paused with the 'r' control.
   const double elapsedSeconds = elapsed / 1000.0;
   const double anemometerDegrees =
      anemometerDegreesPerSecond * windSpeed * elapsedSeconds;
   anemometerAngle =
      std::fmod(anemometerAngle + anemometerDegrees, 360.0);

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
