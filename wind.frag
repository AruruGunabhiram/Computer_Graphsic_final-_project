#version 120

varying float flowPulse;
varying float fogDistance;
varying float ribbonPosition;

uniform float windSpeed;
uniform int fogEnabled;
uniform vec4 fogColor;
uniform float fogStart;
uniform float fogEnd;

void main()
{
   // The fragment shader colors the airflow with a restrained blue-white
   // moving highlight. Smooth end fades keep each strip from looking cut off.
   vec3 airBlue = vec3(0.30, 0.66, 0.88);
   vec3 airWhite = vec3(0.84, 0.95, 1.0);
   float speedRatio = clamp(windSpeed / 5.0, 0.0, 1.0);
   vec3 color = mix(airBlue, airWhite, 0.25 + 0.60 * flowPulse);
   float startFade = smoothstep(0.0, 0.10, ribbonPosition);
   float endFade = 1.0 - smoothstep(0.86, 1.0, ribbonPosition);
   float alpha = gl_Color.a * (0.50 + 0.42 * flowPulse);
   alpha *= startFade * endFade * (0.82 + 0.18 * speedRatio);
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
