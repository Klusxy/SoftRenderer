#version 330 core

uniform sampler2D tex;

in vec2 uv;
out vec4 FragColor;

vec4 GaussianBlur();

void main()
{
	FragColor = texture(tex, uv);
	//FragColor = vec4(1,0,0,1);;
}