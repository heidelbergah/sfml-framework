// paper.frag  -  Desk
//
// Ages a sheet of paper as it is drawn. At 0 it is untouched; at 1 the
// colour has drained out of it and what is left has gone yellow, the
// way newsprint does after a winter.

uniform sampler2D texture;
uniform float     age;

void main()
{
    vec4 c = texture2D(texture, gl_TexCoord[0].xy);

    float grey = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    vec3  aged = vec3(grey) * vec3(1.0, 0.92, 0.76) * 0.9 + vec3(0.05, 0.035, 0.0);

    gl_FragColor = vec4(mix(c.rgb, aged, age), c.a) * gl_Color;
}
