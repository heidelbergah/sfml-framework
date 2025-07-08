uniform sampler2D texture;
uniform float threshold;
uniform float intensity;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec2 texelSize = vec2(1.0 / 300.0, 1.0 / 300.0); // Adjust for resolution

    vec4 original = texture2D(texture, uv);
    vec4 bloom = vec4(0.0);

    // Intensity for each direction
    float w0 = 0.227027 * intensity;
    float w1 = 0.1945946 * intensity;
    float w2 = 0.1216216 * intensity;
    float w3 = 0.054054 * intensity;
    float w4 = 0.016216 * intensity;

    for (int x = -4; x <= 4; ++x)
    {
        for (int y = -4; y <= 4; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec4 color = texture2D(texture, uv + offset);

            // Extract brightness — luminance thresholding
            float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
            if (brightness < threshold)
                color = vec4(0.0); // only bloom bright parts

            float weight = 0.0;
            int ax = abs(x), ay = abs(y);
            if (ax == 0 && ay == 0) weight = w0;
            else if (ax <= 1 && ay <= 1) weight = w1;
            else if (ax <= 2 && ay <= 2) weight = w2;
            else if (ax <= 3 && ay <= 3) weight = w3;
            else weight = w4;

            bloom += color * weight;
        }
    }

    // Apply bloom effect on top of original pixels
    gl_FragColor = original + bloom;
}

