/*
Copyright (c) 2022, Fei Hou and Chiyu Wang, Institute of Software, Chinese Academy of Sciences.
All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <algorithm>
#include "kdtree.h"
#include "utility.h"
#include "PoissonRecon.h"
#include <fstream> 
#include <string> 
#include <sstream>

#include "Camera.h"

using namespace std;

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
std::string vertex_shader_source = readFile("Src/Shader/point_cloud.vert");
std::string fragment_shader_source = readFile("Src/Shader/point_cloud.frag");

Camera camera(glm::vec3(0.0f, 0.0f, 0.0f));
float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

void initOpenGL(GLFWwindow*& window) {
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW" << std::endl;
		exit(-1);
	}
	window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "iPSR Render Approach", NULL, NULL);
	if (!window) {
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		exit(-1);
	}
	glfwMakeContextCurrent(window);
	if (glewInit() != GLEW_OK) {
		std::cerr << "Failed to initialize GLEW" << std::endl;
		exit(-1);
	}
}

void checkOpenGLError(const std::string& context) {
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error in " << context << ": " << err << std::endl;
	}
}

std::string readFile(const std::string& shader_type)
{
	std::ifstream shader_file(shader_type);
	std::stringstream file_content;

	if (shader_file.is_open())
	{
		file_content << shader_file.rdbuf();
		shader_file.close();
	}
	else
	{
		std::cerr << "Could not open the file: " << shader_type << std::endl;
	}

	return file_content.str();
}

GLuint createShaders(const char* vertex_shader_source, const char* fragment_shader_source)
{
	GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vshader, 1, &vertex_shader_source, NULL);
	glCompileShader(vshader);

	GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fshader, 1, &fragment_shader_source, NULL);
	glCompileShader(fshader);


	GLuint program = glCreateProgram();
	glAttachShader(program, vshader);
	glAttachShader(program, fshader);
	glLinkProgram(program);

	GLint program_linked;
	glGetProgramiv(program, GL_LINK_STATUS, &program_linked);
	if (program_linked != GL_TRUE)
	{
		GLsizei log_length = 0;
		GLchar message[1024];
		glGetProgramInfoLog(program, 1024, &log_length, message);
	}

	glDeleteShader(vshader);
	glDeleteShader(fshader);

	return program;

}

GLuint setupBuffers(std::vector<float> flatCoords) {
	GLuint VBO, VAO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, flatCoords.size() * sizeof(float), &flatCoords[0], GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return VAO;
}

void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

void renderScene(const std::vector<Point<double, 3>>& point_cloud_positions)
{

	std::vector<float> flatCoords;
	flatCoords.reserve(point_cloud_positions.size() * 3); // 3 floats per point (x, y, z)

	for (const auto& point : point_cloud_positions) {
		flatCoords.push_back(static_cast<float>(point[0])); // x
		flatCoords.push_back(static_cast<float>(point[1])); // y
		flatCoords.push_back(static_cast<float>(point[2])); // z
	}

	GLFWwindow* window;
	initOpenGL(window);
	GLuint VAO = setupBuffers(flatCoords);
	GLuint program = createShaders(vertex_shader_source.c_str(), fragment_shader_source.c_str());

	glEnable(GL_DEPTH_TEST);
	glfwSetCursorPosCallback(window, mouse_callback);


	while (!glfwWindowShouldClose(window)) {

		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		processInput(window);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(program);

		glm::mat4 view = camera.GetViewMatrix();
		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));


		glBindVertexArray(VAO);
		glDrawArrays(GL_POINTS, 0, flatCoords.size() / 3);
		glBindVertexArray(0);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteProgram(program);
	glDeleteVertexArrays(1, &VAO);

	glfwTerminate();
}

void ipsr(const string& input_name, const string& output_name, int iters, double pointweight, int depth, int k_neighbors)
{
	typedef double REAL;
	const unsigned int DIM = 3U;

	vector<pair<Point<REAL, DIM>, Normal<REAL, DIM>>> points_normals;
	ply_reader<REAL, DIM>(input_name, points_normals);
	std::vector<Point<REAL, DIM>> point_cloud_positions;

	string command = "PoissonRecon --in i.ply --out o.ply --bType 2 --depth " + to_string(depth) + " --pointWeight " + to_string(pointweight);
	vector<string> cmd = split(command);
	vector<char*> argv_str(cmd.size());
	for (size_t i = 0; i < cmd.size(); ++i)
		argv_str[i] = &cmd[i][0];

	XForm<REAL, DIM + 1> iXForm;
	vector<double> weight_samples;
	// sample points by the octree
	points_normals = sample_points<REAL, DIM>((int)argv_str.size(), argv_str.data(), points_normals, iXForm, &weight_samples);

	// initialize normals randomly
	// iterate over all points in point cloud, and set its normal to random values (x,y,z)
	// be sure not to use normals with length 0
	// normalize vector in the end

	// Process points to extract their positions
	for (size_t i = 0; i < points_normals.size(); i++) {
		point_cloud_positions.push_back(points_normals[i].first);
	}

	// Pass the point positions for rendering
	renderScene(point_cloud_positions);


	printf("random initialization...\n");
	Normal<REAL, DIM> zero_normal(Point<REAL, DIM>(0, 0, 0));
	srand(0);
	for (size_t i = 0; i < points_normals.size(); ++i)

	{
		do
		{
			points_normals[i].second = Point<REAL, DIM>(rand() % 1001 - 500.0, rand() % 1001 - 500.0, rand() % 1001 - 500.0);
		} while (points_normals[i].second == zero_normal);
		normalize<REAL, DIM>(points_normals[i].second);
	}

	// construct the Kd-Tree
	kdt::KDTree<kdt::KDTreePoint> tree;
	{
		vector<kdt::KDTreePoint> vertices;
		vertices.reserve(points_normals.size());
		for (size_t i = 0; i < points_normals.size(); ++i)
		{
			array<double, 3> p{ points_normals[i].first[0], points_normals[i].first[1], points_normals[i].first[2] };
			vertices.push_back(kdt::KDTreePoint(p));
		}
		tree.build(vertices);
	}

	pair<vector<Point<REAL, DIM>>, vector<vector<int>>> mesh;

	// iterations
	int epoch = 0;
	while (epoch < iters)
	{
		++epoch;
		printf("Iter: %d\n", epoch);

		vector<Point<REAL, DIM>>().swap(mesh.first);
		vector<vector<int>>().swap(mesh.second);

		// Poisson reconstruction
		mesh = poisson_reconstruction<REAL, DIM>((int)argv_str.size(), argv_str.data(), points_normals, &weight_samples);

		vector<vector<int>> nearestSamples(mesh.second.size());
		vector<Point<REAL, DIM>> normals(mesh.second.size());

		// compute face normals and map them to sample points
#pragma omp parallel for
		for (int i = 0; i < (int)nearestSamples.size(); i++)
		{
			if (mesh.second[i].size() == 3)
			{
				Point<REAL, DIM> c = mesh.first[mesh.second[i][0]] + mesh.first[mesh.second[i][1]] + mesh.first[mesh.second[i][2]];
				c /= 3;
				array<REAL, DIM> a{ c[0], c[1], c[2] };
				nearestSamples[i] = tree.knnSearch(kdt::KDTreePoint(a), k_neighbors);
				normals[i] = Point<REAL, DIM>::CrossProduct(mesh.first[mesh.second[i][1]] - mesh.first[mesh.second[i][0]], mesh.first[mesh.second[i][2]] - mesh.first[mesh.second[i][0]]);
			}
		}

		// update sample point normals
		vector<Normal<REAL, DIM>> projective_normals(points_normals.size(), zero_normal);
		for (size_t i = 0; i < nearestSamples.size(); i++)
		{
			for (size_t j = 0; j < nearestSamples[i].size(); ++j)
			{
				projective_normals[nearestSamples[i][j]].normal[0] += normals[i][0];
				projective_normals[nearestSamples[i][j]].normal[1] += normals[i][1];
				projective_normals[nearestSamples[i][j]].normal[2] += normals[i][2];
			}
		}

#pragma omp parallel for
		for (int i = 0; i < (int)projective_normals.size(); ++i)
			normalize<REAL, DIM>(projective_normals[i]);

		// compute the average normal variation of the top 1/1000 points
		size_t heap_size = static_cast<size_t>(ceil(points_normals.size() / 1000.0));
		priority_queue<double, vector<double>, greater<double>> min_heap;
		for (size_t i = 0; i < points_normals.size(); ++i)
		{
			if (!(projective_normals[i] == zero_normal))
			{
				double diff = Point<REAL, DIM>::SquareNorm((projective_normals[i] - points_normals[i].second).normal);
				if (min_heap.size() < heap_size)
					min_heap.push(diff);
				else if (diff > min_heap.top())
				{
					min_heap.pop();
					min_heap.push(diff);
				}

				points_normals[i].second = projective_normals[i];
			}
		}

		heap_size = min_heap.size();
		double ave_max_diff = 0;
		while (!min_heap.empty())
		{
			ave_max_diff += sqrt(min_heap.top());
			min_heap.pop();
		}
		ave_max_diff /= heap_size;
		printf("normals variation %f\n", ave_max_diff);
		if (ave_max_diff < 0.175)
			break;
	}

	mesh = poisson_reconstruction<REAL, DIM>((int)argv_str.size(), argv_str.data(), points_normals, &weight_samples);

	output_ply(output_name, mesh, iXForm);
	// output_sample_points_and_normals<REAL, DIM>("points_normals_samples.ply", points_normals, iXForm);
	// output_all_points_and_normals<REAL, DIM>("points_normals_all.ply", input_name, points_normals, tree, iXForm);
}

int main(int argc, char* argv[]) {
	// Hardcoded parameters for iPSR
	std::string input_name = "data/bimba.ply";
	std::string output_name = "data/bimba_test.ply";
	int iters = 30;
	double pointweight = 10.0;
	int depth = 10;
	int k_neighbors = 10;

	// ----------------------------
	// command line stuff from authors
	// ----------------------------
	/*
	for (int i = 1; i < argc; i += 2)
	{
		if (strcmp(argv[i], "--in") == 0)
		{
			input_name = argv[i + 1];
			string extension = input_name.substr(min<size_t>(input_name.find_last_of('.'), input_name.length()));
			for (size_t i = 0; i < extension.size(); ++i)
				extension[i] = tolower(extension[i]);
			if (extension != ".ply")
			{
				printf("The input shoud be a .ply file\n");
				return 0;
			}
		}
		else if (strcmp(argv[i], "--out") == 0)
		{
			output_name = argv[i + 1];
			string extension = output_name.substr(min<size_t>(output_name.find_last_of('.'), output_name.length()));
			for (size_t i = 0; i < extension.size(); ++i)
				extension[i] = tolower(extension[i]);
			if (extension != ".ply")
			{
				printf("The output shoud be a .ply file\n");
				return 0;
			}
		}
		else if (strcmp(argv[i], "--iters") == 0)
		{
			long v = strtol(argv[i + 1], nullptr, 10);
			if (!valid_parameter(v))
			{
				printf("invalid value of --iters");
				return 0;
			}
			iters = static_cast<int>(v);
		}
		else if (strcmp(argv[i], "--pointWeight") == 0)
		{
			pointweight = strtod(argv[i + 1], nullptr);
			if (pointweight < 0.0 || pointweight == HUGE_VAL || pointweight == -HUGE_VAL)
			{
				printf("invalid value of --pointWeight");
				return 0;
			}
		}
		else if (strcmp(argv[i], "--depth") == 0)
		{
			long v = strtol(argv[i + 1], nullptr, 10);
			if (!valid_parameter(v))
			{
				printf("invalid value of --depth");
				return 0;
			}
			depth = static_cast<int>(v);
		}
		else if (strcmp(argv[i], "--neighbors") == 0)
		{
			long v = strtol(argv[i + 1], nullptr, 10);
			if (!valid_parameter(v))
			{
				printf("invalid value of --neighbors");
				return 0;
			}
			k_neighbors = static_cast<int>(v);
		}
		else
		{
			printf("unknown parameter of %s\n", argv[i]);
			return 0;
		}
	}
	*/

	// Output settings for debugging
	printf("Iterative Poisson Surface Reconstruction (iPSR)\n");
	printf("Parameters:\n");
	printf("--in          %s\n", input_name.c_str());
	printf("--out         %s\n", output_name.c_str());
	printf("--iters       %d\n", iters);
	printf("--pointWeight %f\n", pointweight);
	printf("--depth       %d\n", depth);
	printf("--neighbors   %d\n\n", k_neighbors);

	// Call ipsr function with hardcoded arguments
	ipsr(input_name, output_name, iters, pointweight, depth, k_neighbors);

	return 0;
}



