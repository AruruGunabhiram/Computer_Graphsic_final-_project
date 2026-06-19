#version 120

varying float flowPulse;
varying float fogDistance;

uniform int fogEnabled;
uniform vec4 fogColor;
uniform float fogStart;
uniform float fogEnd;

void main()
{
   vec3 airBlue = vec3(0.32, 0.72, 1.0);
   vec3 airWhite = vec3(0.92, 0.98, 1.0);
   vec3 color = mix(airBlue, airWhite, flowPulse);
   float alpha = 0.12 + 0.20 * flowPulse;
   vec4 ribbonColor = vec4(color, alpha);

   if (fogEnabled != 0)
   {
      float visibility =
         clamp((fogEnd - fogDistance) / (fogEnd - fogStart), 0.0, 1.0);
      ribbonColor.rgb = mix(fogColor.rgb, ribbonColor.rgb, visibility);
      ribbonColor.a *= 0.55 + 0.45 * visibility;
   }

   gl_FragColor = ribbonColor;
}
