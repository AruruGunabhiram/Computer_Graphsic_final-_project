#version 120

varying float flowPulse;

void main()
{
   vec3 airBlue = vec3(0.32, 0.72, 1.0);
   vec3 airWhite = vec3(0.92, 0.98, 1.0);
   vec3 color = mix(airBlue, airWhite, flowPulse);
   float alpha = 0.12 + 0.20 * flowPulse;
   gl_FragColor = vec4(color, alpha);
}
