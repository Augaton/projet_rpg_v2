#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

// NOTE: Add here your custom variables
uniform vec2 renderSize;

void main()
{
    // Texel size
    vec2 texelSize = vec2(1.0/renderSize.x, 1.0/renderSize.y);
    
    vec4 sum = vec4(0.0);
    
    // Simple 3x3 kernel blur
    sum += texture(texture0, fragTexCoord + vec2(-1.0, -1.0) * texelSize);
    sum += texture(texture0, fragTexCoord + vec2( 0.0, -1.0) * texelSize);
    sum += texture(texture0, fragTexCoord + vec2( 1.0, -1.0) * texelSize);
    sum += texture(texture0, fragTexCoord + vec2(-1.0,  0.0) * texelSize);
    sum += texture(texture0, fragTexCoord + vec2( 0.0,  0.0) * texelSize); // Center
    sum += texture(texture0, fragTexCoord + vec2( 1.0,  0.0) * texelSize);
    sum += texture(texture0, fragTexCoord + vec2(-1.0,  1.0) * texelSize);
    sum += texture(texture0, fragTexCoord + vec2( 0.0,  1.0) * texelSize);
    sum += texture(texture0, fragTexCoord + vec2( 1.0,  1.0) * texelSize);
    
    finalColor = (sum / 9.0) * fragColor * colDiffuse;
}