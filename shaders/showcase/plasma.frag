// plasma.frag  -  Showcase
//
// The classic demoscene plasma: a few overlapping sine fields summed and
// mapped through the palette. Everything comes from time and position;
// no texture is read. The swirl's centre follows the mouse while the
// window is hovered.

uniform vec2  resolution;
uniform float time;
uniform vec2  center;      // 0-1, in the window

// --- palette ramp, shared by every showcase shader -------------------
//
// Four colours from dark to light. A value between two of them is
// resolved with a 4x4 ordered (Bayer) dither instead of a blend, so the
// output only ever contains these four colours: pixel art, not a
// gradient.

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

// ----------------------------------------------------------------------

void main()
{
    vec2 uv    = gl_TexCoord[0].xy;
    vec2 pixel = floor(uv * resolution);

    vec2 p = (uv - center) * vec2(resolution.x / resolution.y, 1.0) * 9.0;

    float v = sin(p.x * 0.9 + time);
    v += sin((p.y * 0.7 + time) * 1.1);
    v += sin((p.x + p.y) * 0.6 + time * 0.7);
    v += sin(length(p) * 1.3 - time * 1.6);

    gl_FragColor = ramp(v * 0.125 + 0.5, pixel);
}
