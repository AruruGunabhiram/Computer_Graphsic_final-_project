#version 120

uniform float time;
uniform float windSpeed;

varying float flowPulse;
varying float fogDistance;
varying float ribbonPosition;

void main()
{
   vec4 position = gl_Vertex;

   float alongRibbon = gl_MultiTexCoord0.x;
   float ribbonPhase = gl_MultiTexCoord0.y;
   float speedRatio = clamp(windSpeed / 5.0, 0.0, 1.0);
   float travel = time * windSpeed * 2.35;
   float waveFrequency = 14.0 + 6.0 * speedRatio;
   float wavePhase = alongRibbon * waveFrequency - travel + ribbonPhase;

   // The vertex shader bends each handmade strip into a moving air current.
   // Higher wind speed advances the wave faster and increases its amplitude,
   // matching the same windSpeed value used for turbine blade RPM in C++.
   position.y += sin(wavePhase) * (0.045 + 0.022 * windSpeed);
   position.z += cos(wavePhase * 0.68) * (0.035 + 0.016 * windSpeed);

   // This pulse travels in the positive-X direction and gives the ribbons a
   // readable flow direction without adding more geometry.
   flowPulse = 0.5 + 0.5 * sin(wavePhase * 1.45);
   ribbonPosition = alongRibbon;
   vec4 eyePosition = gl_ModelViewMatrix * position;
   fogDistance = length(eyePosition.xyz);
   gl_Position = gl_ProjectionMatrix * eyePosition;
   gl_FrontColor = gl_Color;
}
