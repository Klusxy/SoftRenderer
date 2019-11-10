#include <windows.h>
//gl
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Shader.h"
#include "tgaimage.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

struct Vec2i
{
	Vec2i(int _x, int _y)
	{
		x = _x;
		y = _y;
	}
	int x;
	int y;
};

//const
const int g_WindowWidth = 600;
const int g_WindowHeight = 600;
float g_DeltaTime = 0.0f;
float g_LastFrame = 0.0f;

GLFWwindow *g_Window = 0;
GLuint g_QuadVao = 0;
GLuint g_QuadVbo = 0;
GLuint g_SoftRendererFrameBufferTex = 0;

const TGAColor COLOR_RED = TGAColor(255, 0, 0, 255);
const TGAColor COLOR_GREEN = TGAColor(0, 255, 0, 255);
const TGAColor COLOR_BLUE = TGAColor(0, 0, 255, 255);
const TGAColor COLOR_WHITE = TGAColor(255, 255, 255, 255);
TGAImage *g_Image = 0;
const aiScene *g_Scene = 0;

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void process_input(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

bool initWindow();
bool initGLAD();

void createQuad();
void createSoftRendererTexture();

void softRenderer();
// ------------------------------------------------------------
// 填充三角形的软光栅算法
void fillTriangle(Vec2i v0, Vec2i v1, Vec2i v2, TGAImage *image, TGAColor color);
// Digital Differential Analyzer/Bresenham
void drawLineWithBresenham(int x0, int y0, int x1, int y1, TGAImage *image, TGAColor color);
void drawLineWithBresenham(Vec2i v0, Vec2i v1, TGAImage *image, TGAColor color);
void draw_scene(GLFWwindow *window);

void releaseResource();

int main()
{
	if (!initWindow())
		return -1;

	if (!initGLAD())
		return -1;

	createQuad();
	createSoftRendererTexture();

	// soft renderer
	g_Image = new TGAImage(g_WindowWidth, g_WindowHeight, TGAImage::RGBA);

	Assimp::Importer import;
	g_Scene = import.ReadFile("african_head.obj", aiProcess_Triangulate | aiProcess_FlipUVs);
	if (!g_Scene || g_Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !g_Scene->mRootNode)
	{
		std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
		return 0;
	}

	while (!glfwWindowShouldClose(g_Window))
	{
		// per-frame time logic
		// --------------------
		float currentFrame = glfwGetTime();
		g_DeltaTime = currentFrame - g_LastFrame;
		g_LastFrame = currentFrame;
		char windowTitle[50];
		sprintf(windowTitle, "%.3f ms/frame (%.1f FPS)", g_DeltaTime, 1.0f / g_DeltaTime);
		glfwSetWindowTitle(g_Window, windowTitle);

		//event
		glfwPollEvents();
		process_input(g_Window);

		//render
		draw_scene(g_Window);
	}

	releaseResource();

	//release glfw
	glfwTerminate();

	return 0;
}

bool initGLAD()
{
	//init glad
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		return false;

	return true;
}

bool initWindow()
{
	//init glfw
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	g_Window = glfwCreateWindow(g_WindowWidth, g_WindowHeight, "SoftRenderer", NULL, NULL);
	if (g_Window == NULL)
	{
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(g_Window);
	glfwSwapInterval(0);

	//registe callback
	glfwSetFramebufferSizeCallback(g_Window, framebuffer_size_callback);
	glfwSetCursorPosCallback(g_Window, mouse_callback);

	return true;
}

//callback
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}
void process_input(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
}

void createQuad()
{
	Shader shader("./Quad.vs", "./Quad.fs");
	shader.use();

	glViewport(0, 0, g_WindowWidth, g_WindowHeight);

	glm::mat4 model;
	shader.setMat4("model", model);
	glm::mat4 view;
	shader.setMat4("view", view);
	glm::mat4 projection;
	shader.setMat4("projection", projection);

	glGenVertexArrays(1, &g_QuadVao);
	glBindVertexArray(g_QuadVao);

	glGenBuffers(1, &g_QuadVbo);
	glBindBuffer(GL_ARRAY_BUFFER, g_QuadVbo);
	GLfloat vertices[] =
	{
		-1.0f, -1.0f, 0.0f, 0.0f,
		-1.0f, 1.0f, 0.0f, 1.0f,
		1.0f,  -1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (void*)(sizeof(GLfloat) * 2));
	glEnableVertexAttribArray(1);
}

void createSoftRendererTexture()
{
	glGenTextures(1, &g_SoftRendererFrameBufferTex);
	glBindTexture(GL_TEXTURE_2D, g_SoftRendererFrameBufferTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void drawLineWithBresenham(Vec2i v0, Vec2i v1, TGAImage *image, TGAColor color)
{
	bool steep = false;
	if (std::abs(v0.x - v1.x) < std::abs(v0.y - v1.y))
	{
		std::swap(v0.x, v0.y);
		std::swap(v1.x, v1.y);
		steep = true;
	}

	if (v0.x > v1.x)
		std::swap(v0, v1);

	int dx = v1.x - v0.x;
	int dy = v1.y - v0.y;

	float k = std::abs((float)dy / float(dx));
	float distance_y = 0;
	int y = v0.y;

	for (int x = v0.x; x < v1.x; x++)
	{
		if (steep)
			image->set(y, x, color);
		else
			image->set(x, y, color);

		distance_y += k;

		if (distance_y > 0.5)
		{
			y += (v1.y > v0.y ? 1 : -1);
			distance_y -= 1.0;
		}
	}
}

void drawLineWithBresenham(int x0, int y0, int x1, int y1, TGAImage *image, TGAColor color)
{
	// bresenham思路:
	// 遍历时不会出现k>1的情况(k>1的时候会出现断点的情况[也叫孔])，因为如果k>1，从y遍历求x
	// k<0时，遍历的时候会根据y0和y1进行比较，从而选择y是+1还是-1
	// 0.5<k<1时，比较两个像素到y的距离，哪个距离小(近)，选择哪个像素(y表示通过y=kx+b求得的准确y值)
	// 通过比较x0和x1，从而交换位置保证x0<x1(从左到右)

	// 如果k>1,从y遍历
	bool steep = false;
	if (std::abs(x0-x1) < std::abs(y0-y1))
	{
		std::swap(x0, y0);
		std::swap(x1, y1);
		steep = true;
	}

	// 从左到右
	if (x0 > x1)
	{
		std::swap(x0, x1);
		std::swap(y0, y1);
	}

	int dx = x1 - x0;
	int dy = y1 - y0;

	// distance_y表示Ym距离标准y值的距离，如果distance_y>0.5，表示Ym+1比Ym离标准y值离得近
	float k = std::abs((float)dy / float(dx));
	float distance_y = 0;
	// 优化，减少浮点运算
	//int k2 = std::abs(dy) * 2;
	//int distance_y2 = 0;

	int y = y0;

	for (int x = x0; x < x1; x++) 
	{
		if (steep) 
			image->set(y, x, color);
		else 
			image->set(x, y, color);
		
		distance_y += k;
		//distance_y2 += k2;

 		if (distance_y > 0.5) 
		//if (distance_y2 > dx)
		{
			// k<0,如果y1<y0,递减
			y += (y1 > y0 ? 1 : -1);
			distance_y -= 1.0;
			//distance_y2 -= dx * 2;
		}
	}
}

void fillTriangle(Vec2i v0, Vec2i v1, Vec2i v2, TGAImage *image, TGAColor color)
{
	if (v0.y > v1.y)
		std::swap(v0, v1);
	if (v1.y > v2.y)
		std::swap(v1, v2);
	if (v0.y > v1.y)
		std::swap(v0, v1);
	drawLineWithBresenham(v0, v1, image, COLOR_GREEN);
	drawLineWithBresenham(v1, v2, image, COLOR_GREEN);
	drawLineWithBresenham(v2, v0, image, COLOR_RED);
}

void softRenderer()
{
	// step4: 400 * 5 = 2000, 20fps
/*
	for (int i = 0; i < 400; i++)
	{
		// k < 0
		drawLineWithBresenham(0, 0, 600 - 1, 600 - 1, g_Image, COLOR_RED);// k = -1
		// k > 1
		drawLineWithBresenham(0, 600 - 1, 240 - 1, 0, g_Image, COLOR_RED);// 陡峭 k = 2.5
		// k = 1
		drawLineWithBresenham(0, 600 - 1, 600 - 1, 0, g_Image, COLOR_RED);
		drawLineWithBresenham(600 - 1, 0, 0, 600 - 1, g_Image, COLOR_WHITE);// 交换顶点，从右到左
		// 0 < k < 0.5
		drawLineWithBresenham(0, 600 - 1, 600 - 1, 360 - 1, g_Image, COLOR_RED);// 不陡峭 k = 0.4
	}*/
	
	/*for (int i = 0; i < g_Scene->mNumMeshes; i++)
	{
		aiMesh *mesh = g_Scene->mMeshes[i];
		for (int j = 0; j < mesh->mNumFaces; j++)
		{
			aiFace face = mesh->mFaces[j];
			for (int k = 0; k < face.mNumIndices; k++)
			{
				unsigned int index0 = face.mIndices[k];
				unsigned int index1 = face.mIndices[(k + 1) % face.mNumIndices];

				aiVector3D v0 = mesh->mVertices[index0];
				aiVector3D v1 = mesh->mVertices[index1];

				int x0 = (v0.x + 1.0)*g_WindowWidth / 2.0;
				int y0 = (-v0.y + 1.0)*g_WindowHeight / 2.0;
				int x1 = (v1.x + 1.0)*g_WindowWidth / 2.0;
				int y1 = (-v1.y + 1.0)*g_WindowHeight / 2.0;
				drawLineWithBresenham(x0, y0, x1, y1, g_Image, COLOR_WHITE);
			}
		}
	}*/

	Vec2i t0[3] = { Vec2i(10, 70),   Vec2i(50, 160),  Vec2i(70, 80) };
	Vec2i t1[3] = { Vec2i(180, 50),  Vec2i(150, 1),   Vec2i(70, 180) };
	Vec2i t2[3] = { Vec2i(180, 150), Vec2i(120, 160), Vec2i(130, 180) };

	fillTriangle(t0[0], t0[1], t0[2], g_Image, COLOR_RED);
	fillTriangle(t1[0], t1[1], t1[2], g_Image, COLOR_GREEN);
	fillTriangle(t2[0], t2[1], t2[2], g_Image, COLOR_BLUE);

	//g_Image->write_tga_file("output.tga");

	glBindTexture(GL_TEXTURE_2D, g_SoftRendererFrameBufferTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_WindowWidth, g_WindowHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, g_Image->buffer());
	glBindTexture(GL_TEXTURE_2D, 0);
}

void draw_scene(GLFWwindow *window)
{
	softRenderer();

	// 将软渲染结果当作纹理渲染到窗口上
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glBindTexture(GL_TEXTURE_2D, g_SoftRendererFrameBufferTex);
		glBindVertexArray(g_QuadVao);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glfwSwapBuffers(window);
	}
}

void releaseResource()
{
	if (glIsBuffer(g_QuadVbo))
	{
		glDeleteBuffers(1, &g_QuadVbo);
		g_QuadVbo = 0;
	}

	if (glIsVertexArray(g_QuadVao))
	{
		glDeleteVertexArrays(1, &g_QuadVao);
		g_QuadVao = 0;
	}

	if (glIsTexture(g_SoftRendererFrameBufferTex))
	{
		glDeleteTextures(1, &g_SoftRendererFrameBufferTex);
		g_SoftRendererFrameBufferTex = 0;
	}
}