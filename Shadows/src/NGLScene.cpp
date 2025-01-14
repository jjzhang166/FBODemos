#include <QMouseEvent>
#include <QGuiApplication>

#include "NGLScene.h"
#include <ngl/NGLInit.h>
#include <ngl/VAOPrimitives.h>
#include <ngl/ShaderLib.h>
#include <ngl/VAOFactory.h>
#include <ngl/MultiBufferVAO.h>
#include <array>
#include <iostream>

NGLScene::NGLScene()
{
  m_animate = true;
  m_lightPosition.set(8, 4, 8);
  m_lightYPos = 4.0;
  m_lightXoffset = 8.0;
  m_lightZoffset = 8.0;
  setTitle("Simple Shadows");
}

NGLScene::~NGLScene()
{
  std::cout << "Shutting down NGL, removing VAO's and Shaders\n";
}

void NGLScene::resizeGL(int _w, int _h)
{
  m_project = ngl::perspective(45.0f, static_cast<float>(_w) / _h, 0.05f, 350.0f);
  m_win.width = static_cast<int>(_w * devicePixelRatio());
  m_win.height = static_cast<int>(_h * devicePixelRatio());
  m_text->setScreenSize(_w, _h);
}

void NGLScene::initializeGL()
{
  constexpr float znear = 0.1f;
  constexpr float zfar = 100.0f;
  // we must call this first before any other GL commands to load and link the
  // gl commands from the lib, if this is not done program will crash
  ngl::NGLInit::initialize();

  glClearColor(0.4f, 0.4f, 0.4f, 1.0f); // Grey Background
  // enable depth testing for drawing
  glEnable(GL_DEPTH_TEST);
  // enable multisampling for smoother drawing
  glEnable(GL_MULTISAMPLE);

  // Now we will create a basic Camera from the graphics library
  // This is a static camera so it only needs to be set once
  // First create Values for the camera position
  ngl::Vec3 from(0.0f, 2.0f, 6.0f);
  ngl::Vec3 to(0.0f, 0.0f, 0.0f);
  ngl::Vec3 up(0.0f, 1.0f, 0.0f);
  // now load to our new camera
  m_view = ngl::lookAt(from, to, up);
  // set the shape using FOV 45 Aspect Ratio based on Width and Height
  // The final two are near and far clipping planes of 0.5 and 10
  m_project = ngl::perspective(45.0f, 720.0f / 576.0f, znear, zfar);

  // now load to our light POV camera
  m_lightCameraView = ngl::lookAt(m_lightPosition, to, up);
  // here we set the light POV camera shape, the aspect is 1 as our
  // texture is square.
  // use the same clip plane as before but set the FOV a bit larger
  // to get slightly better shadows and the clip planes will help
  // to get rid of some of the artefacts
  m_lightCameraProject = ngl::perspective(65.0f, 1.0f, znear, zfar);

  // in this case I'm only using the light to hold the position
  // it is not passed to the shader directly
  m_lightAngle = 0.0f;

  // we are creating a shader called Texture
  ngl::ShaderLib::createShaderProgram("Texture");
  // now we are going to create empty shaders for Frag and Vert
  ngl::ShaderLib::attachShader("TextureVertex", ngl::ShaderType::VERTEX);
  ngl::ShaderLib::attachShader("TextureFragment", ngl::ShaderType::FRAGMENT);
  // attach the source
  ngl::ShaderLib::loadShaderSource("TextureVertex", "shaders/TextureVert.glsl");
  ngl::ShaderLib::loadShaderSource("TextureFragment", "shaders/TextureFrag.glsl");
  // compile the shaders
  ngl::ShaderLib::compileShader("TextureVertex");
  ngl::ShaderLib::compileShader("TextureFragment");
  // add them to the program
  ngl::ShaderLib::attachShaderToProgram("Texture", "TextureVertex");
  ngl::ShaderLib::attachShaderToProgram("Texture", "TextureFragment");

  // now we have associated this data we can link the shader
  ngl::ShaderLib::linkProgramObject("Texture");
  ngl::ShaderLib::use("Texture");
  ngl::ShaderLib::setUniform("tex", 0);
  // we are creating a shader called Colour
  ngl::ShaderLib::createShaderProgram("Colour");
  // now we are going to create empty shaders for Frag and Vert
  ngl::ShaderLib::attachShader("ColourVertex", ngl::ShaderType::VERTEX);
  ngl::ShaderLib::attachShader("ColourFragment", ngl::ShaderType::FRAGMENT);
  // attach the source
  ngl::ShaderLib::loadShaderSource("ColourVertex", "shaders/ColourVert.glsl");
  ngl::ShaderLib::loadShaderSource("ColourFragment", "shaders/ColourFrag.glsl");
  // compile the shaders
  ngl::ShaderLib::compileShader("ColourVertex");
  ngl::ShaderLib::compileShader("ColourFragment");
  // add them to the program
  ngl::ShaderLib::attachShaderToProgram("Colour", "ColourVertex");
  ngl::ShaderLib::attachShaderToProgram("Colour", "ColourFragment");

  // now we have associated this data we can link the shader
  ngl::ShaderLib::linkProgramObject("Colour");
  ngl::ShaderLib::use("Colour");
  ngl::ShaderLib::setUniform("Colour", 1.0f, 0.0f, 0.0f, 1.0f);

  // we are creating a shader called Shadow
  ngl::ShaderLib::createShaderProgram("Shadow");
  // now we are going to create empty shaders for Frag and Vert
  ngl::ShaderLib::attachShader("ShadowVertex", ngl::ShaderType::VERTEX);
  ngl::ShaderLib::attachShader("ShadowFragment", ngl::ShaderType::FRAGMENT);
  // attach the source
  ngl::ShaderLib::loadShaderSource("ShadowVertex", "shaders/ShadowVert.glsl");
  ngl::ShaderLib::loadShaderSource("ShadowFragment", "shaders/ShadowFrag.glsl");
  // compile the shaders
  ngl::ShaderLib::compileShader("ShadowVertex");
  ngl::ShaderLib::compileShader("ShadowFragment");
  // add them to the program
  ngl::ShaderLib::attachShaderToProgram("Shadow", "ShadowVertex");
  ngl::ShaderLib::attachShaderToProgram("Shadow", "ShadowFragment");

  // now we have associated this data we can link the shader
  ngl::ShaderLib::linkProgramObject("Shadow");
  // create the primitives to draw
  ngl::VAOPrimitives::createSphere("sphere", 0.5, 50);
  ngl::VAOPrimitives::createTorus("torus", 0.15f, 0.4f, 40, 40);

  ngl::VAOPrimitives::createTrianglePlane("plane", 14, 14, 80, 80, ngl::Vec3(0, 1, 0));
  // now create our FBO and texture
  createFramebufferObject();
  // we need to enable depth testing
  // glEnable(GL_DEPTH_TEST);
  // set the depth comparison mode
  glDepthFunc(GL_LEQUAL);
  // set the bg to black
  glClearColor(0, 0, 0, 1.0f);
  // enable face culling this will be switch to front and back when
  // rendering shadow or scene
  glEnable(GL_CULL_FACE);
  glPolygonOffset(1.1f, 4);
  m_text = std::make_unique<ngl::Text>("fonts/Arial.ttf", 14);
  m_text->setColour(1, 1, 1);
  m_text->setScreenSize(width(), height());
  // as re-size is not explicitly called we need to do this.
  // also need to take into account the retina display
  glViewport(0, 0, width() * devicePixelRatio(), height() * devicePixelRatio());
  m_lightTimer = startTimer(40);
}
constexpr int TEXTURE_WIDTH = 1024;
constexpr int TEXTURE_HEIGHT = 1024;

void NGLScene::loadMatricesToShadowShader()
{
  ngl::ShaderLib::use("Shadow");
  ngl::Mat4 MV;
  ngl::Mat4 MVP;
  ngl::Mat3 normalMatrix;
  ngl::Mat4 M;
  M = m_mouseGlobalTX * m_transform.getMatrix();
  MV = m_view * M;
  MVP = m_project * MV;
  normalMatrix = MV;
  normalMatrix.inverse().transpose();
  ngl::ShaderLib::setUniform("MV", MV);
  ngl::ShaderLib::setUniform("MVP", MVP);
  ngl::ShaderLib::setUniform("normalMatrix", normalMatrix);
  ngl::ShaderLib::setUniform("LightPosition", m_lightPosition.m_x, m_lightPosition.m_y, m_lightPosition.m_z);
  ngl::ShaderLib::setUniform("inColour", 1.0f, 1.0f, 1.0f, 1.0f);

  // x = x* 0.5 + 0.5
  // y = y* 0.5 + 0.5
  // z = z* 0.5 + 0.5
  // Moving from unit cube [-1,1] to [0,1]
  auto bias = ngl::Mat4::scale(0.5f, 0.5f, 0.5f);
  // do the translate
  bias.m_m[3][0] = 0.5f;
  bias.m_m[3][1] = 0.5f;
  bias.m_m[3][2] = 0.5f;

  ngl::Mat4 view = m_lightCameraView;
  ngl::Mat4 proj = m_lightCameraProject;
  ngl::Mat4 model = m_transform.getMatrix();

  ngl::Mat4 textureMatrix = bias * proj * view * model;

  ngl::ShaderLib::setUniform("textureMatrix", textureMatrix);
}

void NGLScene::loadToLightPOVShader()
{
  ngl::ShaderLib::use("Colour");
  ngl::Mat4 MVP = m_lightCameraProject * m_lightCameraView * m_transform.getMatrix();
  ngl::ShaderLib::setUniform("MVP", MVP);
}

void NGLScene::drawScene(std::function<void()> _shaderFunc)
{
  // Rotation based on the mouse position for our global transform
  auto rotX = ngl::Mat4::rotateX(m_win.spinXFace);
  auto rotY = ngl::Mat4::rotateY(m_win.spinYFace);
  // multiply the rotations
  m_mouseGlobalTX = rotY * rotX;
  // add the translations
  m_mouseGlobalTX.m_m[3][0] = m_modelPos.m_x;
  m_mouseGlobalTX.m_m[3][1] = m_modelPos.m_y;
  m_mouseGlobalTX.m_m[3][2] = m_modelPos.m_z;

  m_transform.setScale(0.1f, 0.1f, 0.1f);
  m_transform.setPosition(0.0f, -0.5f, 0.0f);
  _shaderFunc();
  ngl::VAOPrimitives::draw("dragon");
  m_transform.reset();
  m_transform.setPosition(-3.0f, 0.0f, 0.0f);
  _shaderFunc();
  ngl::VAOPrimitives::draw("sphere");
  m_transform.reset();
  m_transform.setPosition(3.0f, 0.0f, 0.0f);
  _shaderFunc();
  ngl::VAOPrimitives::draw("cube");
  m_transform.reset();
  m_transform.setPosition(0.0f, 0.0f, 2.0f);
  _shaderFunc();
  ngl::VAOPrimitives::draw("teapot");
  m_transform.reset();
  m_transform.setScale(0.1f, 0.1f, 0.1f);
  m_transform.setPosition(0.0f, -0.5f, -2.0f);
  _shaderFunc();
  ngl::VAOPrimitives::draw("buddah");
  m_transform.reset();
  m_transform.setPosition(2.0f, 0.0f, -2.0f);
  _shaderFunc();
  ngl::VAOPrimitives::draw("torus");
  m_transform.reset();
  m_transform.setPosition(0.0f, -0.5f, 0.0f);
  _shaderFunc();
  ngl::VAOPrimitives::draw("plane");
}

void NGLScene::paintGL()
{
  //----------------------------------------------------------------------------------------------------------------------
  // Pass 1 render our Depth texture to the FBO
  //----------------------------------------------------------------------------------------------------------------------
  // enable culling
  //  glEnable(GL_CULL_FACE);

  // bind the FBO and render offscreen to the texture
  glBindFramebuffer(GL_FRAMEBUFFER, m_fboID);
  // bind the texture object to 0 (off )
  glBindTexture(GL_TEXTURE_2D, 0);
  // we need to render to the same size as the texture to avoid
  // distortions
  glViewport(0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT);

  // Clear previous frame values
  glClear(GL_DEPTH_BUFFER_BIT);
  // as we are only rendering depth turn off the colour / alpha
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  // render only the back faces so we don't get too much self shadowing
  glCullFace(GL_FRONT);
  // draw the scene from the POV of the light using the function we need
  drawScene(std::bind(&NGLScene::loadToLightPOVShader, this));
  //----------------------------------------------------------------------------------------------------------------------
  // Pass two use the texture
  // now we have created the texture for shadows render the scene properly
  //----------------------------------------------------------------------------------------------------------------------
  // go back to our normal framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
  // set the viewport to the screen dimensions
  glViewport(0, 0, width() * devicePixelRatio(), height() * devicePixelRatio());
  // enable colour rendering again (as we turned it off earlier)
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  // clear the screen
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // bind the shadow texture
  glBindTexture(GL_TEXTURE_2D, m_textureID);

  // we need to generate the mip maps each time we bind
  // glGenerateMipmap(GL_TEXTURE_2D);

  // now only cull back faces
  glDisable(GL_CULL_FACE);

  glCullFace(GL_BACK);
  // render our scene with the shadow shader
  drawScene(std::bind(&NGLScene::loadMatricesToShadowShader, this));
  //----------------------------------------------------------------------------------------------------------------------
  // this draws the debug texture on the quad
  //----------------------------------------------------------------------------------------------------------------------

  glBindTexture(GL_TEXTURE_2D, m_textureID);
  debugTexture(-0.6f, -1, 0.6f, 1);
  //----------------------------------------------------------------------------------------------------------------------
  // now we draw a cube to visualise the light
  //----------------------------------------------------------------------------------------------------------------------
  ngl::ShaderLib::use("Colour");
  m_transform.reset();
  m_transform.setPosition(m_lightPosition);
  ngl::Mat4 MVP = m_project * m_view * m_transform.getMatrix();
  ngl::ShaderLib::setUniform("MVP", MVP);
  ngl::VAOPrimitives::draw("cube");

  //----------------------------------------------------------------------------------------------------------------------
  // now draw the text
  //----------------------------------------------------------------------------------------------------------------------
  // we need to do this else the font will not render
  glDisable(GL_CULL_FACE);

  m_text->setColour(1, 1, 1);
  m_text->renderText(250, 700, fmt::format("Light Position [{:0.2f},{:0.2f},{:0.2f}", m_lightPosition.m_x, m_lightPosition.m_y, m_lightPosition.m_z));
  m_text->renderText(250, 680, fmt::format("Y Position {:0.2f}", m_lightYPos));
  m_text->renderText(250, 660, fmt::format("X offset {:0.2f}", m_lightXoffset));
  m_text->renderText(250, 640, fmt::format("Z offset {:0.2f}", m_lightZoffset));
}

void NGLScene::createFramebufferObject()
{

  // Try to use a texture depth component
  glGenTextures(1, &m_textureID);
  glBindTexture(GL_TEXTURE_2D, m_textureID);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);

  glBindTexture(GL_TEXTURE_2D, 0);

  // create our FBO
  glGenFramebuffers(1, &m_fboID);
  glBindFramebuffer(GL_FRAMEBUFFER, m_fboID);
  // disable the colour and read buffers as we only want depth
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);

  // attach our texture to the FBO

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_textureID, 0);

  // switch back to window-system-provided framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//----------------------------------------------------------------------------------------------------------------------

void NGLScene::keyPressEvent(QKeyEvent *_event)
{
  // this method is called every time the main window recives a key event.
  // we then switch on the key value and set the camera in the GLWindow
  switch (_event->key())
  {
  // escape key to quite
  case Qt::Key_Escape:
    QGuiApplication::exit(EXIT_SUCCESS);
    break;
  // turn on wirframe rendering
  case Qt::Key_W:
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    break;
  // turn off wire frame
  case Qt::Key_S:
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    break;
  // show full screen
  case Qt::Key_F:
    showFullScreen();
    break;
  // show windowed
  case Qt::Key_N:
    showNormal();
    break;
  case Qt::Key_Space:
    toggleAnimation();
    break;
  case Qt::Key_Left:
    changeLightXOffset(-0.1f);
    break;
  case Qt::Key_Right:
    changeLightXOffset(0.1f);
    break;
  case Qt::Key_Up:
    changeLightYPos(0.1f);
    break;
  case Qt::Key_Down:
    changeLightYPos(-0.1f);
    break;
  case Qt::Key_I:
    changeLightZOffset(-0.1f);
    break;
  case Qt::Key_O:
    changeLightZOffset(0.1f);
    break;

  default:
    break;
  }
  // finally update the GLWindow and re-draw
  // if (isExposed())
  update();
}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::updateLight()
{
  // change the light angle
  m_lightAngle += 0.05;
  m_lightPosition.set(m_lightXoffset * cos(m_lightAngle), m_lightYPos, m_lightXoffset * sin(m_lightAngle));
  // now set this value and load to the shader
  m_lightCameraView = ngl::lookAt(m_lightPosition, ngl::Vec3(0, 0, 0), ngl::Vec3(0, 1, 0));
}

void NGLScene::timerEvent(QTimerEvent *_event)
{
  // if the timer is the light timer call the update light method
  if (_event->timerId() == m_lightTimer && m_animate == true)
  {
    updateLight();
  }
  // re-draw GL
  update();
}

void NGLScene::debugTexture(float _t, float _b, float _l, float _r)
{
  ngl::ShaderLib::use("Texture");
  ngl::ShaderLib::setUniform("tex", 0);
  glBindTexture(GL_TEXTURE_2D, m_textureID);
  glGenerateMipmap(GL_TEXTURE_2D);
  std::unique_ptr<ngl::AbstractVAO> quad(ngl::VAOFactory::createVAO("multiBufferVAO", GL_TRIANGLES));
  std::array<float, 18> vert; // vertex array
  std::array<float, 12> uv;   // uv array
  vert[0] = _t;
  vert[1] = _l;
  vert[2] = 0.0;
  vert[3] = _t;
  vert[4] = _r;
  vert[5] = 0.0;
  vert[6] = _b;
  vert[7] = _l;
  vert[8] = 0.0;

  vert[9] = _b;
  vert[10] = _l;
  vert[11] = 0.0;
  vert[12] = _t;
  vert[13] = _r;
  vert[14] = 0.0;
  vert[15] = _b;
  vert[16] = _r;
  vert[17] = 0.0;

  uv[0] = 0.0;
  uv[1] = 1.0;
  uv[2] = 1.0;
  uv[3] = 1.0;
  uv[4] = 0.0;
  uv[5] = 0.0;

  uv[6] = 0.0;
  uv[7] = 0.0;
  uv[8] = 1.0;
  uv[9] = 1.0;
  uv[10] = 1.0;
  uv[11] = 0.0;

  quad->bind();

  quad->setData(ngl::AbstractVAO::VertexData(18 * sizeof(GLfloat), vert[0]));
  quad->setVertexAttributePointer(0, 3, GL_FLOAT, 0, 0);
  quad->setData(ngl::AbstractVAO::VertexData(12 * sizeof(GLfloat), uv[0]));
  quad->setVertexAttributePointer(1, 2, GL_FLOAT, 0, 0);
  quad->setNumIndices(6);
  quad->draw();
  quad->unbind();
}
