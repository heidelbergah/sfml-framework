// scope.frag  -  Music lab
//
// An oscilloscope trace of the music. The latest samples arrive as a
// one-pixel-high texture, one sample per column. Each pixel measures its
// distance to the line joining its column's sample to the next, so the
// trace stays continuous however steep it gets, with a soft glow around
// it. Everything is resolved to a four-colour ramp with an ordered
// dither, so the glow is pixel art rather than a gradient.

uniform vec2      resolution;
uniform sampler2D samples;    // r = sample, 0.5 is silence
uniform float     gain;       // how tall the trace is drawn

uniform vec4 c0;
uniform vec4 c1;
uniform vec4 c2;
uniform vec4 c3;

float bayer2(vec2 a)
{
    a = floor(a);
    return fract(a.x / 2.0 + a.y * a.y * 0.75);
}

float bayer4(vec2 a)
{
    return bayer2(0.5 * a) * 0.25 + bayer2(a);
}

vec4 ramp(float value, vec2 pixel)
{
    float scaled = clamp(value, 0.0, 1.0) * 3.0;
    float index  = floor(scaled);
    if(fract(scaled) > bayer4(pixel)) index += 1.0;

    if(index < 0.5) return c0;
    if(index < 1.5) return c1;
    if(index < 2.5) return c2;
    return c3;
}

float trace_y(float column)
{
    float s = texture2D(samples, vec2((column + 0.5) / resolution.x, 0.5)).r * 2.0 - 1.0;
    return (0.5 - clamp(s * gain, -1.0, 1.0) * 0.46) * resolution.y;
}

void main()
{
    vec2 pixel = floor(gl_TexCoord[0].xy * resolution);

    float a  = trace_y(pixel.x);
    float b  = trace_y(min(pixel.x + 1.0, resolution.x - 1.0));
    float lo = min(a, b) - 0.5;
    float hi = max(a, b) + 0.5;

    float y = pixel.y + 0.5;
    float d = max(max(lo - y, y - hi), 0.0);

    float line = (d < 1.0) ? 1.0 : exp(-(d - 1.0) * 0.4) * 0.55;

    // Graticule: twelve columns, four rows, and a brighter centre line.
    float gx   = mod(pixel.x, resolution.x / 12.0);
    float gy   = mod(pixel.y, resolution.y / 4.0);
    float grid = (gx < 1.0 || gy < 1.0) ? 0.18 : 0.0;
    if(abs(pixel.y - floor(resolution.y / 2.0)) < 0.5) grid = 0.3;

    gl_FragColor = ramp(max(line, grid), pixel);
}
