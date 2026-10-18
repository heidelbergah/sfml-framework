// ripple.frag  -  Showcase
//
// Shades a water surface. The wave simulation itself runs on the CPU and
// arrives as a height texture; this pass works out the slope at each
// pixel, bends the tiled floor underneath by it, and adds a glint where
// the surface tilts toward the light.

uniform vec2      resolution;
uniform sampler2D heights;    // r = height, 0.5 is still water

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

float height(vec2 uv)
{
    return texture2D(heights, uv).r * 2.0 - 1.0;
}

void main()
{
    vec2 uv    = gl_TexCoord[0].xy;
    vec2 pixel = floor(uv * resolution);
    vec2 texel = 1.0 / resolution;

    float dx = height(uv + vec2(texel.x, 0.0)) - height(uv - vec2(texel.x, 0.0));
    float dy = height(uv + vec2(0.0, texel.y)) - height(uv - vec2(0.0, texel.y));

    // The floor, seen through the water: 16px tiles, displaced by slope.
    vec2  seen  = pixel + vec2(dx, dy) * 24.0;
    vec2  cell  = mod(seen, 16.0);
    float grout = (cell.x < 1.0 || cell.y < 1.0) ? 1.0 : 0.0;
    float check = mod(floor(seen.x / 16.0) + floor(seen.y / 16.0), 2.0);

    // Still water sits exactly on the second colour, so only the
    // ripples dither; tiles alternate faintly and grout drops a level.
    float value = 0.334 + check * 0.05 - grout * 0.16;

    // Light from the top left.
    value += clamp(-(dx + dy) * 2.2, 0.0, 1.0) * 0.75;
    value -= clamp((dx + dy) * 1.2, 0.0, 1.0) * 0.18;

    gl_FragColor = ramp(value, pixel);
}
