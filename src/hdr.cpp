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

// TYPE OF HDR IMPLEMENTED
enum IlluminationType {
  NO_HDR = 0,
  REINHARD_HDR = 1,
  EXPONENTIAL_HDR = 2,
  DRAGO_HDR = 3
};

// STRUCTURE OF FRAME ILLUMINATION DATA
struct Illumination{
    float exposure; //how much time "camera" absorbed light
    enum IlluminationType hdr; //type of hdr
    bool dynamicExposure; //dynamic exposure for global-type hdr for change dynamically exposure and create local-like behaviour
    bool bloom; //blurring effect of lights
    float adaptationSpeed; //how fast you adapt from dark to light and viceversa
    float maxChange; //limit how much you can adapt frame by frame

    float avgPixelScreenLuminance; // average pixel luminance of a frame
    float maxPixelScreenLuminance; // maximum pixel luminance of a frame
    float minPixelScreenLuminance; // minimum pixel luminance of a frame

    float infCapLuminance; // inferior cap of luminance : when surpassed we increase exposure gradually to maxExposure
    float supCapLuminance; // superior cap of luminance : when surpassed we decrease exposure gradually to minExposure
    
    float avgExposure; // exposure of frame between inferior and superior cap of luminance
    float minExposure; // minimum exposure of a scene
    float maxExposure; // maximum exposure of a scene
};

// parses json file of a config file
std::ifstream conf_file("settings/config.json");
json config = json::parse(conf_file);

// WINDOW SETTINGS
unsigned int win_width = config["window"]["width"]; //width of the window
unsigned int win_height = config["window"]["height"]; //height of the window

// CAMERA SETTINGS
Camera camera(glm::vec3(config["camera"]["x"], config["camera"]["y"], config["camera"]["z"]));
float lastX = (float)win_width / 2.0;
float lastY = (float)win_height / 2.0;
bool firstMouse = true;

// TIMING VARIABLES
float deltaTimeFrame = 0.0f; //difference of time from a frame and his predecessor
float lastFrame = 0.0f; //absolute time of the precedessor frame from the start of the program
float currentFrame = 0.0f; //absolute time of the actual frame from the start of the program 

// FUNCTION DECLARATIONS
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processWindowInput(GLFWwindow *window);
void processIlluminationInput(GLFWwindow* window, Illumination* illum, bool* illuminationChangeKeyPressed, bool* dynamicExposureKeyPressed , bool* bloomKeyPressed);
unsigned int loadTexture(const char *path, bool gammaCorrection);
unsigned int loadCubemapSkyboxTexture(std::vector<std::string> faces);
float* calculateLuminanceScreenStats(float* imageFrameData, int width, int height);
void updateExposure(Illumination* illum);

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

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
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // ILLUMINATION SETTINGS
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
    illum_settings.adaptationSpeed = config["illumination"]["adaptation_speed"];
    illum_settings.maxChange = config["illumination"]["max_change"];
    illum_settings.bloom = config["illumination"]["bloom"];
    bool illuminationChangeKeyPressed = false;
    bool dynamicExposureKeyPressed = false;
    bool bloomKeyPressed = false;

    // GLOBAL OPERATIONS
    glEnable(GL_DEPTH_TEST);

    // SHADERS
    //shader definitions
    Shader lightingShader("shader/lightVS.txt", "shader/lightFS.txt"); //for rendering container and lights
    Shader skyboxShader("shader/skyboxVS.txt", "shader/skyboxFS.txt"); //for rendering skybox
    Shader blurShader("shader/blurVS", "shader/blurFS"); //for blooming (post-processing operation)
    Shader hdrShader("shader/hdrVS.txt", "shader/hdrFS.txt"); //for hdr (post-processing operation)

    
    lightingShader.useProgram();
    lightingShader.setInt("diffuseTexture", 0);
    hdrShader.useProgram();
    hdrShader.setInt("hdrBuffer", 0);
    skyboxShader.useProgram();
    skyboxShader.setInt("skybox", 0);

    // VAOs & VBOs (VertexArrayObjects & VertexBufferObjects)
    //SkyBox settings
    unsigned int skyboxVAO;
    unsigned int skyboxVBO;
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
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    //Container settings
    unsigned int containerVAO;
    unsigned int containerVBO;
    float containerVertices[] = {
        /*
        // back face
        -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
        1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
        1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
        -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
        */
        // front face
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
    glGenVertexArrays(1, &containerVAO);
    glGenBuffers(1, &containerVBO);
    glBindBuffer(GL_ARRAY_BUFFER, containerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(containerVertices), containerVertices, GL_STATIC_DRAW);
    glBindVertexArray(containerVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); //positions
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); //normals
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); //texcoords
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    //Frame settings (useful for post-processing operations)
    unsigned int frameVAO;
    unsigned int frameVBO;
    float frameVertices[] = {
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    glGenVertexArrays(1, &frameVAO);
    glGenBuffers(1, &frameVBO);
    glBindVertexArray(frameVAO);
    glBindBuffer(GL_ARRAY_BUFFER, frameVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(frameVertices), &frameVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0); //positions
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))); //texcoords
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // TEXTURES
    unsigned int containerTexture = loadTexture(std::filesystem::path("resources/textures/container.png").string().c_str(), true); // note that we're loading the texture as an SRGB texture
    std::vector<std::string> skyboxFaces{
        std::filesystem::path("resources/skybox/px.jpg").string(),
        std::filesystem::path("resources/skybox/nx.jpg").string(),
        std::filesystem::path("resources/skybox/py.jpg").string(),
        std::filesystem::path("resources/skybox/ny.jpg").string(),
        std::filesystem::path("resources/skybox/pz.jpg").string(),
        std::filesystem::path("resources/skybox/nz.jpg").string(),
    };
    unsigned int skyboxTexture = loadCubemapSkyboxTexture(skyboxFaces);

    // FBOs (FrameBufferObjects)
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    unsigned int colorBuffers[2]; //creates two images : one for color and the other for brightness 
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, win_width, win_height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    
    //create depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, win_width, win_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    //Color attachments we'll use (of this framebuffer) for rendering 
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffers for two-pass gaussian blurring (first horizontally and than vertically)
    // these framebuffers keep passing data from one to another
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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

    // LIGHTS
    // light positions
    std::vector<glm::vec3> lightPositions;
    lightPositions.push_back(glm::vec3( 49.5f,  49.5f, -255.5f)); //sun
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
    
    //light colors
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

    // IMAGE FRAME DATA
    float* imageFrameData = new float[win_width * win_height * 3]; 

    // RENDER LOOP
    while (!glfwWindowShouldClose(window))
    {
        // TIME CALCULATIONS FRAME BY FRAME
        currentFrame = static_cast<float>(glfwGetTime());
        deltaTimeFrame = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // CAMERA VIEW & PERSPECTIVE
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (GLfloat)win_width / (GLfloat)win_height, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // INPUT PROCESSING
        processWindowInput(window);
        processIlluminationInput(window, &illum_settings, &illuminationChangeKeyPressed, &dynamicExposureKeyPressed, &bloomKeyPressed);

        // WHITE SCREEN RENDERING (if something doesn't work with rendering, we obtain only a white window)
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // FRAMEBUFFER RENDERING (CONTAINER)
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        lightingShader.useProgram();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, containerTexture);
        for (unsigned int i = 0; i < lightPositions.size(); i++)
        {
            lightingShader.setVec3("lights[" + std::to_string(i) + "].Position", lightPositions[i]);
            lightingShader.setVec3("lights[" + std::to_string(i) + "].Color", lightColors[i]);
        }
        lightingShader.setVec3("viewPos", camera.Position);
        glm::mat4 containerModel = glm::mat4(1.0f);
        containerModel = glm::translate(containerModel, glm::vec3(0.0f, 0.0f, -15.0));
        containerModel = glm::scale(containerModel, glm::vec3(3.0f, 3.0f, 27.5f));
        lightingShader.setMat4("model", containerModel);
        lightingShader.setInt("inverse_normals", true);
        glBindVertexArray(containerVAO);
        glDrawArrays(GL_TRIANGLES, 0, 30);
        glBindVertexArray(0);

        // SKYBOX RENDERING
        glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
        skyboxShader.useProgram();
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

        glReadPixels(0, 0, win_width, win_height, GL_RGB, GL_FLOAT, imageFrameData);  
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // BLOOM FILTER
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;
        blurShader.useProgram();
        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader.setInt("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
            glBindVertexArray(frameVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // LUMINANCE STATS
        float* luminanceScreenStats = calculateLuminanceScreenStats(imageFrameData,win_width,win_height);
        illum_settings.avgPixelScreenLuminance = luminanceScreenStats[0];
        illum_settings.maxPixelScreenLuminance = luminanceScreenStats[1];
        illum_settings.minPixelScreenLuminance = luminanceScreenStats[2];
        if(illum_settings.dynamicExposure){
            updateExposure(&illum_settings);
        }

        // POST-PROCESSING OPERATIONS (HDR RENDERING)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        hdrShader.useProgram();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);//Apply FB color texture
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);//Apply Bloom Filter texture 
        hdrShader.setInt("hdr", illum_settings.hdr);
        hdrShader.setInt("bloom", illum_settings.bloom);
        hdrShader.setFloat("exposure", illum_settings.exposure);
        //Drago-only Tone-Mapping uniform variables
        hdrShader.setFloat("maxPixelScreenLuminance", illum_settings.maxPixelScreenLuminance);
        hdrShader.setFloat("avgPixelScreenLuminance", illum_settings.avgPixelScreenLuminance);
        glBindVertexArray(frameVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        std::cout << "hdr: " << illum_settings.hdr << "| dynamicExp: " << (illum_settings.dynamicExposure ? "on" : "off") << "| bloom: " << (illum_settings.bloom ? "on" : "off") << "| exposure: " << illum_settings.exposure << std::endl;

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// FUNCTION DEFINITIONS

// process window input: whether relevant keys are pressed/released window change (only camera and window feature)
// ---------------------------------------------------------------------------------------------------------------
void processWindowInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTimeFrame);

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTimeFrame);

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTimeFrame);

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTimeFrame);
}

// process illumination input: whether relevant keys are pressed/released illumination change
// ---------------------------------------------------------------------------------------------------------
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

// Whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions
    glViewport(0, 0, width, height);
}

// Whenever the mouse moves, this callback is called
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
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// Whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// Utility function for loading a 2D texture from file
// --------------------------------------------------------
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

// Utility function for loading skybox from faces
// ----------------------------------------------------------------------
unsigned int loadCubemapSkyboxTexture(std::vector<std::string> faces)
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

// Utility function for calculate average, maximum and minimum luminance of a screen frame
// -----------------------------------------------------------------------------------------
float* calculateLuminanceScreenStats(float* imageFrameData, int width, int height) {
    static float luminanceStats[3]; //avg=0 max=1 min=2
    luminanceStats[1]=-1;
    luminanceStats[2]=-1;
    float totalLuminance = 0.0f;
    for (int i = 0; i < width * height * 3; i += 3) {
        // Calculate pixel luminance with perceptive formula
        float pixelLuminance = 0.2126f * imageFrameData[i] + 0.7152f * imageFrameData[i + 1] + 0.0722f * imageFrameData[i + 2];
        totalLuminance += pixelLuminance;
        // Find max luminance
        if(pixelLuminance>luminanceStats[1] || luminanceStats[1]==-1){
            luminanceStats[1]=pixelLuminance;
        }
        // Find min luminance
        if(pixelLuminance<luminanceStats[2] || luminanceStats[2]==-1){
            luminanceStats[2]=pixelLuminance;
        }
    }
    // Calculate avg luminance
    luminanceStats[0] = totalLuminance / (width * height);
    return luminanceStats;
}

void updateExposure(Illumination* illum){
    // Target of ideal value of exposure
    float targetExposure =  (*illum).avgExposure;
    // If average pixel luminance is lower than an inferior cap (dark scene) then increase exposure 
    // in a non-linear way
    if ((*illum).avgPixelScreenLuminance < (*illum).infCapLuminance) {
        targetExposure = pow((*illum).infCapLuminance / (*illum).avgPixelScreenLuminance, 1.5f);
    }
    // If average pixel luminance is greater than a superior cap (light scene) then decrease exposure 
    // in a non-linear way
    else if ((*illum).avgPixelScreenLuminance > (*illum).supCapLuminance) {
        targetExposure = pow((*illum).supCapLuminance / (*illum).avgPixelScreenLuminance, 1.5f);
    }

    // This formula describes exposure change based on frame by frame difference with a learning rate
    float exposureChange = (targetExposure - (*illum).exposure) * (*illum).adaptationSpeed * deltaTimeFrame;
    // Limit exposure change into a range [-maxChange,maxChange] for limiting simil-instatanous frame 
    // by frame average pixel luminance change
    exposureChange = std::clamp(exposureChange, -(*illum).maxChange, (*illum).maxChange);
    // Adding exposure change to image exposure
    (*illum).exposure += exposureChange;
    // Limit exposure into a range [minExposure,maxExposure] for infinite exposure in dark scene 
    // behaviour and viceversa
    (*illum).exposure = std::clamp((*illum).exposure, (*illum).minExposure, (*illum).maxExposure);
}