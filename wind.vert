#version 120

uniform float time;
uniform float windSpeed;

void main()
{
   // Keep both future animation uniforms active without changing geometry yet.
   float reservedWindOffset = 0.0000001 * (time + windSpeed);
   vec4 position = gl_Vertex;
   position.x += reservedWindOffset;
   gl_Position = gl_ModelViewProjectionMatrix * position;
   gl_FrontColor = gl_Color;
}
