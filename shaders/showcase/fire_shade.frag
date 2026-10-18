// fire_shade.frag  -  Showcase
//
// Turns the fire's heat field into colour. The simulation runs at half
// the window's resolution; sampling it without smoothing keeps each
// heat cell a crisp 2x2 block, and the dither runs at full resolution
// inside them.

uniform vec2      resolution;
uniform sampler2D heat;

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

    float h = texture2D(heat, vec2(uv.x, 1.0 - uv.y)).r;

    gl_FragColor = ramp(h * 0.97, pixel);
}
