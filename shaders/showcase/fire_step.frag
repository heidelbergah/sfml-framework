// fire_step.frag  -  Showcase
//
// One step of the fire simulation, run into a render texture that is
// read back on the next frame: the shader feeds itself.
//
// Each pixel takes the heat of the pixel just below it, its row nudged
// left or right as a whole, and loses a random amount. The bottom row is kept hot,
// so heat keeps rising and cooling into flames.
//
// Render textures are stored upside down, which is why every read flips
// y. Reads and writes agree, so "below" stays below.

uniform vec2      resolution;
uniform sampler2D previous;
uniform float     seed;
uniform float     step;        // counts simulation steps
uniform vec2      fan_pos;     // 0-1, in the window
uniform float     fan;         // 0 when the mouse is elsewhere

// Hash without sine (Dave Hoskins). The usual fract(sin(x) * 43758.5)
// loses precision on many GPUs once x grows, and the "noise" turns into
// visible stripes.
float hash(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Smooth value noise: random values on a grid, blended between.
float value_noise(vec2 p)
{
    vec2 cell = floor(p);
    vec2 f    = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash(cell);
    float b = hash(cell + vec2(1.0, 0.0));
    float c = hash(cell + vec2(0.0, 1.0));
    float d = hash(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float heat_at(vec2 pixel)
{
    vec2 uv = (pixel + 0.5) / resolution;
    return texture2D(previous, vec2(uv.x, 1.0 - uv.y)).r;
}

void main()
{
    vec2 pixel = floor(gl_TexCoord[0].xy * resolution);

    float r = hash(pixel + vec2(seed * 61.0, seed * 17.0));

    float heat;
    if(pixel.y >= resolution.y - 1.0)
    {
        // A steady source. Each row above is effectively an older copy
        // of this one, so a source that changed every step would paint
        // a different stripe into every row; the flames come from the
        // uneven cooling instead.
        heat = 1.0;
    }
    else
    {
        // Drift sideways a whole row at a time, like wind. Choosing it
        // per pixel made neighbours read the same pixel below, so their
        // histories merged and smeared the heat into horizontal streaks.
        float gust  = hash(vec2(pixel.y * 1.7, seed * 13.0));
        float drift = floor(gust * 3.0) - 1.0;
        heat = heat_at(pixel + vec2(drift, 1.0));

        // Cooling comes mostly from a smooth noise field that rises at
        // the same speed as the heat (one row per step), so a column of
        // flame keeps meeting the same patch of it: columns in a cool
        // patch die early, their neighbours shoot up, and you get
        // tongues. Independent per-pixel cooling just averages out into
        // a noisy gradient.
        float field = value_noise(vec2(pixel.x / 4.5, (pixel.y + step) / 11.0));
        heat -= 0.010 + field * field * 0.075 + r * 0.008;
    }

    // The mouse adds a blob of heat while it is over the window.
    if(fan > 0.0)
    {
        float d = distance(pixel, fan_pos * resolution);
        heat = max(heat, fan * (1.0 - smoothstep(2.0, 7.0, d)));
    }

    heat = clamp(heat, 0.0, 1.0);
    gl_FragColor = vec4(heat, heat, heat, 1.0);
}
