#version 330 core
layout(location = 0) in vec2 iPos;
layout(location = 1) in vec2 iTex;
out vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(iPos.x, iPos.y, -1, 1.0f);
	uv = iTex;
}