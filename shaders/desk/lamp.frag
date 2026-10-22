// lamp.frag  -  Desk
//
// A pool of warm light from a desk lamp, multiplied over everything.
// The falloff is stepped into four levels with an ordered dither rather
// than a smooth gradient, so the light reads as pixel art too.

uniform vec2 resolution;
uniform vec2 light;        // centre of the pool, in pixels

float bayer2(vec2 a)
{
    a = floor(a);
    return fract(a.x / 2.0 + a.y * a.y * 0.75);
}

float bayer4(vec2 a)
{
    return bayer2(0.5 * a) * 0.25 + bayer2(a);
}

void main()
{
    vec2  pixel = floor(gl_TexCoord[0].xy * resolution);
    float d     = distance(pixel, light) / resolution.y;
    float v     = 1.0 - smoothstep(0.18, 0.95, d);

    float scaled = v * 3.0;
    float level  = floor(scaled);
    if(fract(scaled) > bayer4(pixel)) level += 1.0;

    float brightness = mix(0.52, 1.0, level / 3.0);
    gl_FragColor = vec4(brightness, brightness * 0.95, brightness * 0.86, 1.0);
}
