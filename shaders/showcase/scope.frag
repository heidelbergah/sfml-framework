// scope.frag  -  Showcase
//
// An oscilloscope trace. The latest audio arrives as a one-pixel-high
// texture, one sample per column. Each pixel measures how far it is
// from the line joining its column's sample to the next one, so the
// trace is a continuous line however steep it gets, with a soft glow
// around it.

uniform vec2      resolution;
uniform sampler2D samples;    // r = sample, 0.5 is silence
uniform float     level;      // 0 hides the trace, 1 shows it fully

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

float trace_y(float column)
{
    float s = texture2D(samples, vec2((column + 0.5) / resolution.x, 0.5)).r * 2.0 - 1.0;
    return (0.5 - s * 0.9) * resolution.y;
}

void main()
{
    vec2 uv    = gl_TexCoord[0].xy;
    vec2 pixel = floor(uv * resolution);

    float a  = trace_y(pixel.x);
    float b  = trace_y(min(pixel.x + 1.0, resolution.x - 1.0));
    float lo = min(a, b) - 0.5;
    float hi = max(a, b) + 0.5;

    float y = pixel.y + 0.5;
    float d = max(max(lo - y, y - hi), 0.0);

    float line = (d < 1.0) ? 1.0 : exp(-(d - 1.0) * 0.45) * 0.55;
    line *= level;

    // A faint graticule: eight columns, four rows.
    float gx   = mod(pixel.x, resolution.x / 8.0);
    float gy   = mod(pixel.y, resolution.y / 4.0);
    float grid = (gx < 1.0 || gy < 1.0) ? 0.2 : 0.0;

    gl_FragColor = ramp(max(line, grid), pixel);
}
