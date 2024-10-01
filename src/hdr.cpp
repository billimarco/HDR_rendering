#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <fstream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <nlohmann/json.hpp>

#include <shader.h>
#include <camera.h>

using json = nlohmann::json;

enum IlluminationType {
  NO_HDR = 0,
  REINHARD_HDR = 1,
  EXPONENTIAL_HDR = 2,
  DRAGO_HDR = 3
};

struct Illumination{
    float exposure;
    enum IlluminationType hdr;
    bool dynamicExposure;
    bool bloom;
    float adaptationSpeed;
    float maxChange;

    float avgPixelScreenLuminance;
    float maxPixelScreenLuminance;
    float minPixelScreenLuminance;

    float infCapLuminance;
    float supCapLuminance;
    
    float avgExposure;
    float minExposure;
    float maxExposure;
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processWindowInput(GLFWwindow *window);
void processIlluminationInput(GLFWwindow* window, Illumination* illum, bool* illuminationChangeKeyPressed, bool* dynamicExposureKeyPressed , bool* bloomKeyPressed);
unsigned int loadTexture(const char *path, bool gammaCorrection);
unsigned int loadCubemapSkybox(std::vector<std::string> faces);
float* calculateLuminanceScreenStats(float* imageData, int width, int height);
void updateExposure(Illumination* illum);

// fa il parse del file json in ingresso e ottengo una struttura simile ad un dizionario con tutti i valori del file di config
std::ifstream conf_file("settings/config.json");
json config = json::parse(conf_file);

// SCREEN SETTINGS
unsigned int win_width = config["window"]["width"];
unsigned int win_height = config["window"]["height"];

// CAMERA SETTINGS
Camera camera(glm::vec3(config["camera"]["x"], config["camera"]["y"], config["camera"]["z"]));
float lastX = (float)win_width / 2.0;
float lastY = (float)win_height / 2.0;
bool firstMouse = true;

// TIMING VARIABLES
float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    //ILLUMINATION SETTINGS
    Illumination illum_settings;
    illum_settings.hdr = config["illumination"]["type"];
    illum_settings.dynamicExposure = config["illumination"]["dynamic_exp"];
    illum_settings.exposure = config["illumination"]["exposure"];
    illum_settings.infCapLuminance = config["illumination"]["inf_cap_luminance"];
    illum_settings.supCapLuminance = config["illumination"]["sup_cap_luminance"];
    illum_settings.avgExposure = config["illumination"]["avg_exposure"];
    illum_settings.minExposure = config["illumination"]["min_exposure"];
    illum_settings.minExposure = config["illumination"]["min_exposure"];
    illum_settings.maxExposure = config["illumination"]["max_exposure"];
    illum_settings.adaptationSpeed = config["illumination"]["adaptation_speed"];// Smoothly adapt exposure (adjust speed as needed)
    illum_settings.maxChange = config["illumination"]["max_change"];
    illum_settings.bloom = config["illumination"]["bloom"];
    bool illuminationChangeKeyPressed = false;
    bool dynamicExposureKeyPressed = false;
    bool bloomKeyPressed = false;

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(win_width, win_height, "HDR_rendering_Elaborato", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile shaders
    // -------------------------
    Shader lightingShader("shader/lightingVS.txt", "shader/lightingFS.txt");
    Shader skyboxShader("shader/skyboxVS.txt", "shader/skyboxFS.txt");
    Shader blurShader("shader/blurVS", "shader/blurFS");
    Shader hdrShader("shader/hdrVS.txt", "shader/hdrFS.txt");

    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f,  1.0f
    };
    // skybox VAO
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    unsigned int cubeVAO;
    unsigned int cubeVBO;
    float vertices[] = {
        /*// back face
        -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
        1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
        1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
        -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
        */// front face
        -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
        1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
        1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
        1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
        -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
        -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
        // left face
        -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
        -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
        -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
        -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
        -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
        -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
        // right face
        1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
        1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
        1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
        1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
        1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
        1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
        // bottom face
        -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
        1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
        1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
        1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
        -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
        -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
        // top face
        -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
        1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
        1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
        1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
        -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
        -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left 
            
    };
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    // fill buffer
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // link vertex attributes
    glBindVertexArray(cubeVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    unsigned int quadVAO;
    unsigned int quadVBO;
    float quadVertices[] = {
        // positions        // texture Coords
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    // setup plane VAO
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // load textures
    // -------------
    unsigned int containerTexture = loadTexture(std::filesystem::path("resources/textures/container.png").string().c_str(), true); // note that we're loading the texture as an SRGB texture
    std::vector<std::string> skyboxFaces{
        std::filesystem::path("resources/skybox/right.jpg").string(),
        std::filesystem::path("resources/skybox/left.jpg").string(),
        std::filesystem::path("resources/skybox/top.jpg").string(),
        std::filesystem::path("resources/skybox/bottom.jpg").string(),
        std::filesystem::path("resources/skybox/front.jpg").string(),
        std::filesystem::path("resources/skybox/back.jpg").string(),
    };
    unsigned int skyboxTexture = loadCubemapSkybox(skyboxFaces);

    // configure floating point framebuffer
    // ------------------------------------
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    // create floating point color buffer
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, win_width, win_height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    // create depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, win_width, win_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, win_width, win_height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        // also check if framebuffers are complete (no need for depth buffer)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

    // lighting info
    // -------------
    // positions
    std::vector<glm::vec3> lightPositions;
    lightPositions.push_back(glm::vec3( 0.0f,  49.5f, -255.5f)); // sun
    lightPositions.push_back(glm::vec3( 0.0f, 0.0f, -40.5f));
    lightPositions.push_back(glm::vec3( 2.5f, 0.0f, -22.5f));
    lightPositions.push_back(glm::vec3( 0.0f, -2.5f, -22.5f));
    lightPositions.push_back(glm::vec3( -2.5f, 0.0f, -22.5f));
    lightPositions.push_back(glm::vec3( 0.0f, 2.5f, -22.5f));
    lightPositions.push_back(glm::vec3( 2.5f, 0.0f, -15.0f));
    lightPositions.push_back(glm::vec3( 0.0f, -2.5f, -15.0f));
    lightPositions.push_back(glm::vec3( -2.5f, 0.0f, -15.0f));
    lightPositions.push_back(glm::vec3( 0.0f, 2.5f, -15.0f));
    lightPositions.push_back(glm::vec3( 2.5f, 0.0f, -7.5f));
    lightPositions.push_back(glm::vec3( 0.0f, -2.5f, -7.5f));
    lightPositions.push_back(glm::vec3( -2.5f, 0.0f, -7.5f));
    lightPositions.push_back(glm::vec3( 0.0f, 2.5f, -7.5f));
    // colors
    std::vector<glm::vec3> lightColors;
    lightColors.push_back(glm::vec3(300.0f, 300.0f, 300.0f));
    lightColors.push_back(glm::vec3(200.0f, 200.0f, 200.0f));
    lightColors.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
    lightColors.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
    lightColors.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
    lightColors.push_back(glm::vec3(1.0f, 1.0f, 0.0f));
    lightColors.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
    lightColors.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
    lightColors.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
    lightColors.push_back(glm::vec3(1.0f, 1.0f, 0.0f));
    lightColors.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
    lightColors.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
    lightColors.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
    lightColors.push_back(glm::vec3(1.0f, 1.0f, 0.0f));

    // shader configuration
    // --------------------
    lightingShader.use();
    lightingShader.setInt("diffuseTexture", 0);
    hdrShader.use();
    hdrShader.setInt("hdrBuffer", 0);
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    float* imageData = new float[win_width * win_height * 3];

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processWindowInput(window);
        processIlluminationInput(window, &illum_settings, &illuminationChangeKeyPressed, &dynamicExposureKeyPressed, &bloomKeyPressed);

        // render
        // ------
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. render scene into floating point framebuffer
        // -----------------------------------------------
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (GLfloat)win_width / (GLfloat)win_height, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        lightingShader.use();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, containerTexture);
        // set lighting uniforms
        for (unsigned int i = 0; i < lightPositions.size(); i++)
        {
            lightingShader.setVec3("lights[" + std::to_string(i) + "].Position", lightPositions[i]);
            lightingShader.setVec3("lights[" + std::to_string(i) + "].Color", lightColors[i]);
        }
        lightingShader.setVec3("viewPos", camera.Position);
        // render tunnel
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, -15.0));
        model = glm::scale(model, glm::vec3(3.0f, 3.0f, 27.5f));
        lightingShader.setMat4("model", model);
        lightingShader.setInt("inverse_normals", true);
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 30);
        glBindVertexArray(0);

        glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
        skyboxShader.use();
        view = glm::mat4(glm::mat3(camera.GetViewMatrix())); // remove translation from the view matrix
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, win_width, win_height, GL_RGB, GL_FLOAT, imageData);  
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. blur bright fragments with two-pass Gaussian Blur 
        // --------------------------------------------------
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;
        blurShader.use();
        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader.setInt("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        float* luminanceScreenStats = calculateLuminanceScreenStats(imageData,win_width,win_height);
        illum_settings.avgPixelScreenLuminance = luminanceScreenStats[0];
        illum_settings.maxPixelScreenLuminance = luminanceScreenStats[1];
        illum_settings.minPixelScreenLuminance = luminanceScreenStats[2];

        if(illum_settings.dynamicExposure){
            updateExposure(&illum_settings);
        }
        // 2. now render floating point color buffer to 2D quad and tonemap HDR colors to default framebuffer's (clamped) color range
        // --------------------------------------------------------------------------------------------------------------------------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        hdrShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
        hdrShader.setInt("hdr", illum_settings.hdr);
        hdrShader.setInt("bloom", illum_settings.bloom);
        hdrShader.setFloat("exposure", illum_settings.exposure);
        //Drago-only uniform variables
        hdrShader.setFloat("maxPixelScreenLuminance", illum_settings.maxPixelScreenLuminance);
        hdrShader.setFloat("avgPixelScreenLuminance", illum_settings.avgPixelScreenLuminance);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        std::cout << "hdr: " << illum_settings.hdr << "| dynamicExp: " << (illum_settings.dynamicExposure ? "on" : "off") << "| bloom: " << (illum_settings.bloom ? "on" : "off") << "| exposure: " << illum_settings.exposure << std::endl;
        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    // Pulizia e uscita
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processWindowInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

void processIlluminationInput(GLFWwindow* window, Illumination* illum, bool* illuminationChangeKeyPressed, bool* dynamicExposureKeyPressed, bool* bloomKeyPressed)
{
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS && !(*illuminationChangeKeyPressed))
    {
        (*illum).hdr = NO_HDR;
        *illuminationChangeKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !(*illuminationChangeKeyPressed))
    {
        (*illum).hdr = REINHARD_HDR;
        *illuminationChangeKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !(*illuminationChangeKeyPressed))
    {
        (*illum).hdr = EXPONENTIAL_HDR;
        *illuminationChangeKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && !(*illuminationChangeKeyPressed))
    {
        (*illum).hdr = DRAGO_HDR;
        *illuminationChangeKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_RELEASE || glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE || glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE)
    {
        *illuminationChangeKeyPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !(*dynamicExposureKeyPressed))
    {
        (*illum).dynamicExposure = !(*illum).dynamicExposure;
        *dynamicExposureKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE)
    {
        *dynamicExposureKeyPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !(*bloomKeyPressed))
    {
        (*illum).bloom = !(*illum).bloom;
        *bloomKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE)
    {
        *bloomKeyPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        if ((*illum).exposure > 0.0f)
            (*illum).exposure -= 0.001f;
        else
            (*illum).exposure = 0.0f;
    }
    else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        (*illum).exposure += 0.001f;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
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
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const * path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum internalFormat;
        GLenum dataFormat;
        if (nrComponents == 1)
        {
            internalFormat = dataFormat = GL_RED;
        }
        else if (nrComponents == 3)
        {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        }
        else if (nrComponents == 4)
        {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

unsigned int loadCubemapSkybox(std::vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrComponents;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrComponents, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

float* calculateLuminanceScreenStats(float* imageData, int width, int height) {
    static float luminanceStats[3]; //avg=0 max=1 min=2
    luminanceStats[1]=-1;
    luminanceStats[2]=-1;
    float totalLuminance = 0.0f;
    for (int i = 0; i < width * height * 3; i += 3) {
        float r = imageData[i];
        float g = imageData[i + 1];
        float b = imageData[i + 2];
        // Calcola la luminanza utilizzando la formula percettiva
        float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        totalLuminance += luminance;
        if(luminance>luminanceStats[1] || luminanceStats[1]==-1){
            luminanceStats[1]=luminance;
        }
        if(luminance<luminanceStats[2] || luminanceStats[2]==-1){
            luminanceStats[2]=luminance;
        }
    }
    luminanceStats[0] = totalLuminance / (width * height);
    return luminanceStats;
}

void updateExposure(Illumination* illum){
    // Il target può essere il valore ideale di esposizione che si desidera
    float targetExposure =  (*illum).avgExposure;
    // Se la luminosità media è inferiore al range ideale (scena troppo buia)
    if ((*illum).avgPixelScreenLuminance < (*illum).infCapLuminance) {
        // Aumenta l'esposizione in modo non lineare: più è buio, più l'esposizione aumenta
        targetExposure = pow((*illum).infCapLuminance / (*illum).avgPixelScreenLuminance, 1.5f); // Il valore 1.5 regola la sensibilità
    }
    // Se la luminosità media è superiore al range ideale (scena troppo luminosa)
    else if ((*illum).avgPixelScreenLuminance > (*illum).supCapLuminance) {
        // Riduci l'esposizione in modo non lineare: più è luminosa la scena, più riduci l'esposizione
        targetExposure = pow((*illum).supCapLuminance / (*illum).avgPixelScreenLuminance, 1.5f);
    }

    float exposureChange = (targetExposure - (*illum).exposure) * (*illum).adaptationSpeed * deltaTime;

    // Limita l'esposizione per evitare valori estremi
    if (fabs(exposureChange) > (*illum).maxChange) {
        // Se la differenza è maggiore del cambiamento massimo consentito
        if (exposureChange > 0) {
            exposureChange = (*illum).maxChange;
        } else {
            exposureChange = -(*illum).maxChange;
        }
    }

    (*illum).exposure += exposureChange;
    (*illum).exposure = std::clamp((*illum).exposure, (*illum).minExposure, (*illum).maxExposure);
}

//DEVI TROVARE LA MAXLUMINANCE E FARE UN TONE-MAPPING