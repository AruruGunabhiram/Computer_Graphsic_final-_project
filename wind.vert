#version 120

uniform float time;
uniform float windSpeed;

varying float flowPulse;
varying float fogDistance;

void main()
{
   vec4 position = gl_Vertex;

   float alongRibbon = gl_MultiTexCoord0.x;
   float ribbonPhase = gl_MultiTexCoord0.y;
   float travel = time * windSpeed * 2.2;
   float wavePhase = alongRibbon * 20.0 - travel + ribbonPhase;

   position.y += sin(wavePhase) * (0.10 + 0.025 * windSpeed);
   position.z += cos(wavePhase * 0.72) * (0.08 + 0.018 * windSpeed);

   flowPulse = 0.5 + 0.5 * sin(wavePhase * 1.35);
   vec4 eyePosition = gl_ModelViewMatrix * position;
   fogDistance = length(eyePosition.xyz);
   gl_Position = gl_ProjectionMatrix * eyePosition;
   gl_FrontColor = gl_Color;
}
