#include "common.h"
#include "atb.h"

// Includes
#include <cstdio>
#include <cstdlib>

using namespace v8;
using namespace node;

#include <iostream>
#include <sstream>

using namespace std;

namespace glfw {

/* @Module: GLFW initialization, termination and version querying */

NAN_METHOD(Init) {
  NanScope();
  NanReturnValue(JS_BOOL(glfwInit()==1));
}

NAN_METHOD(Terminate) {
  NanScope();
  glfwTerminate();
  NanReturnUndefined();
}

NAN_METHOD(GetVersion) {
  NanScope();
  int major, minor, rev;
  glfwGetVersion(&major,&minor,&rev);
  Local<Array> arr=Array::New(v8::Isolate::GetCurrent(),3);
  arr->Set(JS_STR("major"),JS_INT(major));
  arr->Set(JS_STR("minor"),JS_INT(minor));
  arr->Set(JS_STR("rev"),JS_INT(rev));
  NanReturnValue(arr);
}

NAN_METHOD(GetVersionString) {
  NanScope();
  const char* ver=glfwGetVersionString();
  NanReturnValue(JS_STR(ver));
}

/* @Module: Time input */

NAN_METHOD(GetTime) {
  NanScope();
  NanReturnValue(JS_NUM(glfwGetTime()));
}

NAN_METHOD(SetTime) {
  NanScope();
  double time = args[0]->NumberValue();
  glfwSetTime(time);
  NanReturnUndefined();
}

/* @Module: monitor handling */

/* TODO: Monitor configuration change callback */

NAN_METHOD(GetMonitors) {
  NanScope();
  int monitor_count, mode_count, xpos, ypos, width, height;
  int i, j;
  GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);
  GLFWmonitor *primary = glfwGetPrimaryMonitor();
  const GLFWvidmode *mode, *modes;
  
  Local<Array> js_monitors = Array::New(v8::Isolate::GetCurrent(),monitor_count);
  Local<Object> js_monitor, js_mode;
  Local<Array> js_modes;
  for(i=0; i<monitor_count; i++){
    js_monitor = Object::New(v8::Isolate::GetCurrent());
    js_monitor->Set(JS_STR("is_primary"), JS_BOOL(monitors[i] == primary));
    js_monitor->Set(JS_STR("index"), JS_INT(i));
    
    js_monitor->Set(JS_STR("name"), JS_STR(glfwGetMonitorName(monitors[i])));
    
    glfwGetMonitorPos(monitors[i], &xpos, &ypos);
    js_monitor->Set(JS_STR("pos_x"), JS_INT(xpos));
    js_monitor->Set(JS_STR("pos_y"), JS_INT(ypos));
    
    glfwGetMonitorPhysicalSize(monitors[i], &width, &height);
    js_monitor->Set(JS_STR("width_mm"), JS_INT(width));
    js_monitor->Set(JS_STR("height_mm"), JS_INT(height));
    
    mode = glfwGetVideoMode(monitors[i]);
    js_monitor->Set(JS_STR("width"), JS_INT(mode->width));
    js_monitor->Set(JS_STR("height"), JS_INT(mode->height));
    js_monitor->Set(JS_STR("rate"), JS_INT(mode->refreshRate));
    
    modes = glfwGetVideoModes(monitors[i], &mode_count);
    js_modes = Array::New(v8::Isolate::GetCurrent(),mode_count);
    for(j=0; j<mode_count; j++){
      js_mode = Object::New(v8::Isolate::GetCurrent());
      js_mode->Set(JS_STR("width"), JS_INT(modes[j].width));
      js_mode->Set(JS_STR("height"), JS_INT(modes[j].height));
      js_mode->Set(JS_STR("rate"), JS_INT(modes[j].refreshRate));
      // NOTE: Are color bits necessary?
      js_modes->Set(JS_INT(j), js_mode);
    }
    js_monitor->Set(JS_STR("modes"), js_modes);
    
    js_monitors->Set(JS_INT(i), js_monitor);
  }
  
  NanReturnValue(js_monitors);
}

/* @Module: Window handling */
Persistent<Object> glfw_events;
int lastX=0,lastY=0;
bool windowCreated=false;

void NAN_INLINE(CallEmitter(int argc, Handle<Value> argv[])) {
  NanScope();
  // MakeCallback(glfw_events, "emit", argc, argv);
  if(NanNew(glfw_events)->Has(NanSymbol("emit"))) {
    Local<Function> callback = NanNew(glfw_events)->Get(NanSymbol("emit")).As<Function>();

    if (!callback.IsEmpty()) {
      callback->Call(NanGetCurrentContext()->Global(),argc,argv);
    }
  }
}

/* Window callbacks handling */
void APIENTRY windowPosCB(GLFWwindow *window, int xpos, int ypos) {
  NanScope();
  //cout<<"resizeCB: "<<w<<" "<<h<<endl;

  Local<Array> evt=Array::New(v8::Isolate::GetCurrent(),3);
  evt->Set(JS_STR("type"),JS_STR("window_pos"));
  evt->Set(JS_STR("xpos"),JS_INT(xpos));
  evt->Set(JS_STR("ypos"),JS_INT(ypos));

  Handle<Value> argv[2] = {
    JS_STR("window_pos"), // event name
    evt
  };

  CallEmitter(2, argv);
}

void APIENTRY windowSizeCB(GLFWwindow *window, int w, int h) {
  NanScope();
  //cout<<"resizeCB: "<<w<<" "<<h<<endl;

  Local<Array> evt=Array::New(v8::Isolate::GetCurrent(),3);
  evt->Set(JS_STR("type"),JS_STR("resize"));
  evt->Set(JS_STR("width"),JS_INT(w));
  evt->Set(JS_STR("height"),JS_INT(h));

  Handle<Value> argv[2] = {
    JS_STR("resize"), // event name
    evt
  };

  CallEmitter(2, argv);
}

void APIENTRY windowFramebufferSizeCB(GLFWwindow *window, int w, int h) {
  NanScope();
  //cout<<"resizeCB: "<<w<<" "<<h<<endl;

  Local<Array> evt=Array::New(v8::Isolate::GetCurrent(),3);
  evt->Set(JS_STR("type"),JS_STR("framebuffer_resize"));
  evt->Set(JS_STR("width"),JS_INT(w));
  evt->Set(JS_STR("height"),JS_INT(h));

  Handle<Value> argv[2] = {
    JS_STR("framebuffer_resize"), // event name
    evt
  };

  CallEmitter(2, argv);
}

void APIENTRY windowCloseCB(GLFWwindow *window) {
  NanScope();

  Handle<Value> argv[1] = {
    JS_STR("quit"), // event name
  };

  CallEmitter(2, argv);
}

void APIENTRY windowRefreshCB(GLFWwindow *window) {
  NanScope();

  Local<Array> evt=Array::New(v8::Isolate::GetCurrent(),2);
  evt->Set(JS_STR("type"),JS_STR("refresh"));
  evt->Set(JS_STR("window"),JS_NUM((uint64_t) window));

  Handle<Value> argv[2] = {
    JS_STR("refresh"), // event name
    evt
  };

  CallEmitter(2, argv);
}

void APIENTRY windowIconifyCB(GLFWwindow *window, int iconified) {
  NanScope();

  Local<Array> evt=Array::New(v8::Isolate::GetCurrent(),2);
  evt->Set(JS_STR("type"),JS_STR("iconified"));
  evt->Set(JS_STR("iconified"),JS_BOOL(iconified));

  Handle<Value> argv[2] = {
    JS_STR("iconified"), // event name
    evt
  };

  CallEmitter(2, argv);
}

void APIENTRY windowFocusCB(GLFWwindow *window, int focused) {
  NanScope();

  Local<Array> evt=Array::New(v8::Isolate::GetCurrent(),2);
  evt->Set(JS_STR("type"),JS_STR("focused"));
  evt->Set(JS_STR("focused"),JS_BOOL(focused));

  Handle<Value> argv[2] = {
    JS_STR("focused"), // event name
    evt
  };

  CallEmitter(2, argv);
}

static int jsKeyCode[]={
/*GLFW_KEY_ESCAPE*/       27,
/*GLFW_KEY_ENTER*/        13,
/*GLFW_KEY_TAB*/          9,
/*GLFW_KEY_BACKSPACE*/    8,    
/*GLFW_KEY_INSERT*/       45,
/*GLFW_KEY_DELETE*/       46,
/*GLFW_KEY_RIGHT*/        39,
/*GLFW_KEY_LEFT*/         37,
/*GLFW_KEY_DOWN*/         40,
/*GLFW_KEY_UP*/           38,
/*GLFW_KEY_PAGE_UP*/      33,  
/*GLFW_KEY_PAGE_DOWN*/    34,   
/*GLFW_KEY_HOME*/         36,
/*GLFW_KEY_END*/          35,
/*GLFW_KEY_CAPS_LOCK*/    20,    
/*GLFW_KEY_SCROLL_LOCK*/  145,      
/*GLFW_KEY_NUM_LOCK*/     144,   
/*GLFW_KEY_PRINT_SCREEN*/ 144, /* TODO */       
/*GLFW_KEY_PAUSE*/        19,
/*GLFW_KEY_F1*/           112,
/*GLFW_KEY_F2*/           113,
/*GLFW_KEY_F3*/           114,
/*GLFW_KEY_F4*/           115,
/*GLFW_KEY_F5*/           116,
/*GLFW_KEY_F6*/           117,
/*GLFW_KEY_F7*/           118,
/*GLFW_KEY_F8*/           119,
/*GLFW_KEY_F9*/           120,
/*GLFW_KEY_F10*/          121,   
/*GLFW_KEY_F11*/          122,
/*GLFW_KEY_F12*/          123,
/*GLFW_KEY_F13*/          123, /* unknown */
/*GLFW_KEY_F14*/          123, /* unknown */
/*GLFW_KEY_F15*/          123, /* unknown */
/*GLFW_KEY_F16*/          123, /* unknown */
/*GLFW_KEY_F17*/          123, /* unknown */
/*GLFW_KEY_F18*/          123, /* unknown */
/*GLFW_KEY_F19*/          123, /* unknown */
/*GLFW_KEY_F20*/          123, /* unknown */
/*GLFW_KEY_F21*/          123, /* unknown */
/*GLFW_KEY_F22*/          123, /* unknown */
/*GLFW_KEY_F23*/          123, /* unknown */
/*GLFW_KEY_F24*/          123, /* unknown */
/*GLFW_KEY_F25*/          123, /* unknown */
/*GLFW_KEY_KP_0*/         96,
/*GLFW_KEY_KP_1*/         97,
/*GLFW_KEY_KP_2*/         98,
/*GLFW_KEY_KP_3*/         99,
/*GLFW_KEY_KP_4*/         100,
/*GLFW_KEY_KP_5*/         101,
/*GLFW_KEY_KP_6*/         102,
/*GLFW_KEY_KP_7*/         103,
/*GLFW_KEY_KP_8*/         104,
/*GLFW_KEY_KP_9*/         105,
/*GLFW_KEY_KP_DECIMAL*/   110,
/*GLFW_KEY_KP_DIVIDE*/    111,
/*GLFW_KEY_KP_MULTIPLY*/  106, 
/*GLFW_KEY_KP_SUBTRACT*/  109, 
/*GLFW_KEY_KP_ADD*/       107,
/*GLFW_KEY_KP_ENTER*/     13,
/*GLFW_KEY_KP_EQUAL*/     187,
/*GLFW_KEY_LEFT_SHIFT*/   16,
/*GLFW_KEY_LEFT_CONTROL*/ 17,     
/*GLFW_KEY_LEFT_ALT*/     18,   
/*GLFW_KEY_LEFT_SUPER*/   91,   
/*GLFW_KEY_RIGHT_SHIFT*/  16,     
/*GLFW_KEY_RIGHT_CONTROL*/17,       
/*GLFW_KEY_RIGHT_ALT*/    18,   
/*GLFW_KEY_RIGHT_SUPER*/  93,      
/*GLFW_KEY_MENU*/         18
};

void APIENTRY keyCB(GLFWwindow *window, int key, int scancode, int action, int mods) {
  const char *actionNames = "keyup\0  keydown\0keypress";

  if(!TwEventKeyGLFW(key,action)) {
    NanScope();

    Local<Array> evt=Array::New(v8::Isolate::GetCurrent(),7);
    evt->Set(JS_STR("type"), JS_STR( &actionNames[action << 3] ));
    evt->Set(JS_STR("ctrlKey"),JS_BOOL(mods & GLFW_MOD_CONTROL));
    evt->Set(JS_STR("shiftKey"),JS_BOOL(mods & GLFW_MOD_SHIFT));
    evt->Set(JS_STR("altKey"),JS_BOOL(mods & GLFW_MOD_ALT));
    evt->Set(JS_STR("metaKey"),JS_BOOL(mods & GLFW_MOD_SUPER));

    int which=key, charCode=key;

    if(key>=GLFW_KEY_ESCAPE && key<=GLFW_KEY_LAST)
      key=jsKeyCode[key-GLFW_KEY_ESCAPE];
    else if(key==GLFW_KEY_SEMICOLON)  key=186;    // ;
    else if(key==GLFW_KEY_EQUAL)  key=187;        // =
    else if(key==GLFW_KEY_COMMA)  key=188;        // ,
    else if(key==GLFW_KEY_MINUS)  key=189;        // -
    else if(key==GLFW_KEY_PERIOD)  key=190;       // .
    else if(key==GLFW_KEY_SLASH)  key=191;        // /
    else if(key==GLFW_KEY_GRAVE_ACCENT)  key=192; // `
    else if(key==GLFW_KEY_LEFT_BRACKET)  key=219; // [
    else if(key==GLFW_KEY_BACKSLASH)  key=220;    /* \ */
    else if(key==GLFW_KEY_RIGHT_BRACKET)  key=221;// ]
    else if(key==GLFW_KEY_APOSTROPHE)  key=222;   // '

    evt->Set(JS_STR("which"),JS_INT(which));
    evt->Set(JS_STR("keyCode"),JS_INT(key));
    evt->Set(JS_STR("charCode"),JS_INT(charCode));

    Handle<Value> argv[2] = {
      JS_STR(&actionNames[action << 3]), // event name
      evt
    };

    CallEmitter(2, argv);
  }
}

void APIENTRY cursorPosCB(GLFWwindow* window, double x, double y) {
  if(!TwEventMousePosGLFW(x,y)) {
    int w,h;
    glfwGetWindowSize(window, &w, &h);
    if(x<0 || x>=w) return;
    if(y<0 || y>=h) return;

    lastX=x;
    lastY=y;

    NanScope();

    Local<Array> evt=Array::New(v8::Isolate::GetCurrent(),5);
    evt->Set(JS_STR("type"),JS_STR("mousemove"));
    evt->Set(JS_STR("pageX"),JS_INT(x));
    evt->Set(JS_STR("pageY"),JS_INT(y));
    evt->Set(JS_STR("x"),JS_INT(x));
    evt->Set(JS_STR("y"),JS_INT(y));

    Handle<Value> argv[2] = {
      JS_STR("mousemove"), // event name
      evt
    };

    CallEmitter(2, argv);
  }
}

void APIENTRY cursorEnterCB(GLFWwindow* window, int entered) {
  NanScope();

  Local<Array> evt=Array::New(v8::Isolate::GetCurrent(),2);
  evt->Set(JS_STR("type"),JS_STR("mouseenter"));
  evt->Set(JS_STR("entered"),JS_INT(entered));

  Handle<Value> argv[2] = {
    JS_STR("mouseenter"), // event name
    evt
  };

  CallEmitter(2, argv);
}

void APIENTRY mouseButtonCB(GLFWwindow *window, int button, int action, int mods) {
   if(!TwEventMouseButtonGLFW(button,action)) {
    NanScope();
    Local<Array> evt=Array::New(v8::Isolate::GetCurrent(),7);
    evt->Set(JS_STR("type"),JS_STR(action ? "mousedown" : "mouseup"));
    evt->Set(JS_STR("button"),JS_INT(button));
    evt->Set(JS_STR("which"),JS_INT(button));
    evt->Set(JS_STR("x"),JS_INT(lastX));
    evt->Set(JS_STR("y"),JS_INT(lastY));
    evt->Set(JS_STR("pageX"),JS_INT(lastX));
    evt->Set(JS_STR("pageY"),JS_INT(lastY));

    Handle<Value> argv[2] = {
      JS_STR(action ? "mousedown" : "mouseup"), // event name
      evt
    };

    CallEmitter(2, argv);
  }
}

void APIENTRY scrollCB(GLFWwindow *window, double xoffset, double yoffset) {
  if(!TwEventMouseWheelGLFW(yoffset)) {
    NanScope();

    Local<Array> evt=Array::New(v8::Isolate::GetCurrent(),3);
    evt->Set(JS_STR("type"),JS_STR("mousewheel"));
    evt->Set(JS_STR("wheelDeltaX"),JS_INT(xoffset*120));
    evt->Set(JS_STR("wheelDeltaY"),JS_INT(yoffset*120));
    evt->Set(JS_STR("wheelDelta"),JS_INT(yoffset*120));

    Handle<Value> argv[2] = {
      JS_STR("mousewheel"), // event name
      evt
    };

    CallEmitter(2, argv);
  }
}

int APIENTRY windowCloseCB() {
  NanScope();

  Handle<Value> argv[1] = {
    JS_STR("quit"), // event name
  };

  CallEmitter(1, argv);

  return 1;
}

NAN_METHOD(testJoystick) {
  NanScope();

  int width = args[0]->Uint32Value();
  int height = args[1]->Uint32Value();
  float ratio = width / (float) height;

  float translateX = args[2]->NumberValue();
  float translateY = args[3]->NumberValue();
  float translateZ = args[4]->NumberValue();

  float rotateX = args[5]->NumberValue();
  float rotateY = args[6]->NumberValue();
  float rotateZ = args[7]->NumberValue();

  float angle = args[8]->NumberValue();

  glViewport(0, 0, width, height);
  glClear(GL_COLOR_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);
  glMatrixMode(GL_MODELVIEW);

  glLoadIdentity();
  glRotatef(angle, rotateX, rotateY, rotateZ);
  glTranslatef(translateX, translateY, translateZ);

  glBegin(GL_TRIANGLES);
  glColor3f(1.f, 0.f, 0.f);
  glVertex3f(-0.6f, -0.4f, 0.f);
  glColor3f(0.f, 1.f, 0.f);
  glVertex3f(0.6f, -0.4f, 0.f);
  glColor3f(0.f, 0.f, 1.f);
  glVertex3f(0.f, 0.6f, 0.f);
  glEnd();

  NanReturnUndefined();
}

NAN_METHOD(testScene) {
  NanScope();
  int width = args[0]->Uint32Value();
  int height = args[1]->Uint32Value();
  float z = args.Length()>2 ? (float) args[2]->NumberValue() : 0;
  float ratio = width / (float) height;

  glViewport(0, 0, width, height);
  glClear(GL_COLOR_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);
  glMatrixMode(GL_MODELVIEW);

  glLoadIdentity();
  glRotatef((float) glfwGetTime() * 50.f, 0.f, 0.f, 1.f);

  glBegin(GL_TRIANGLES);
  glColor3f(1.f, 0.f, 0.f);
  glVertex3f(-0.6f+z, -0.4f, 0.f);
  glColor3f(0.f, 1.f, 0.f);
  glVertex3f(0.6f+z, -0.4f, 0.f);
  glColor3f(0.f, 0.f, 1.f);
  glVertex3f(0.f+z, 0.6f, 0.f);
  glEnd();

  NanReturnUndefined();
}

NAN_METHOD(WindowHint) {
  NanScope();
  int target       = args[0]->Uint32Value();
  int hint         = args[1]->Uint32Value();
  glfwWindowHint(target, hint);
  NanReturnUndefined();
}

NAN_METHOD(DefaultWindowHints) {
  NanScope();
  glfwDefaultWindowHints();
  NanReturnUndefined();
}

NAN_METHOD(JoystickPresent) {
  NanScope();
  int joy = args[0]->Uint32Value();
  bool isPresent = glfwJoystickPresent(joy);
  NanReturnValue(JS_BOOL(isPresent));
}

std::string intToString(int number) {
  std::ostringstream buff;
  buff << number;
  return buff.str();
}

std::string floatToString(float number){
    std::ostringstream buff;
    buff<<number;
    return buff.str();
}

std::string buttonToString(unsigned char c) {
  int number = (int)c;
  return intToString(number);
}

NAN_METHOD(GetJoystickAxes) {
  NanScope();
  int joy = args[0]->Uint32Value();
  int count;
  const float *axisValues = glfwGetJoystickAxes(joy, &count);
  string response = "";
  for (int i = 0; i < count; i++) {
    response.append(floatToString(axisValues[i]));
    response.append(","); //Separator
  }

  NanReturnValue(JS_STR(response.c_str()));
}

NAN_METHOD(GetJoystickButtons) {
  NanScope();
  int joy = args[0]->Uint32Value();
  int count = 0;
  const unsigned char* response = glfwGetJoystickButtons(joy, &count);

  string strResponse = "";
  for (int i = 0; i < count; i++) {
    strResponse.append(buttonToString(response[i]));
    strResponse.append(",");
  }

  NanReturnValue(JS_STR(strResponse.c_str()));
}

NAN_METHOD(GetJoystickName) {
  NanScope();
  int joy = args[0]->Uint32Value();
  const char* response = glfwGetJoystickName(joy);
  NanReturnValue(JS_STR(response));
}

NAN_METHOD(glfw_CreateWindow) {
  NanScope();
  int width       = args[0]->Uint32Value();
  int height      = args[1]->Uint32Value();
  String::Utf8Value str(args[2]->ToString());
  int monitor_idx = args[3]->Uint32Value();
  
  GLFWwindow* window = NULL;
  GLFWmonitor **monitors = NULL, *monitor = NULL;
  int monitor_count;
  if(args.Length() >= 4 && monitor_idx >= 0){
    monitors = glfwGetMonitors(&monitor_count);
    if(monitor_idx >= monitor_count){
      return NanThrowError("Invalid monitor");
    }
    monitor = monitors[monitor_idx];
  }

  if(!windowCreated) {
    window = glfwCreateWindow(width, height, *str, monitor, NULL);

    if(!window) {
      // can't create window, throw error
      return NanThrowError("Can't create GLFW window");
    }

    glfwMakeContextCurrent(window);

    // make sure cursor is always shown
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    GLenum err = glewInit();
    if (err)
    {
      /* Problem: glewInit failed, something is seriously wrong. */
      string msg="Can't init GLEW (glew error ";
      msg+=(const char*) glewGetErrorString(err);
      msg+=")";

      fprintf(stderr, "%s", msg.c_str());
      return NanThrowError(msg.c_str());
    }
    fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
  }
  else
    glfwSetWindowSize(window, width,height);

  // Set callback functions
  // glfw_events=Persistent<Object>::New(args.This()->Get(JS_STR("events"))->ToObject());
  //NanInitPersistent(Object,_events,args.This()->Get(JS_STR("events"))->ToObject());
  NanAssignPersistent(glfw_events, args.This()->Get(JS_STR("events"))->ToObject());

  // window callbacks
  glfwSetWindowPosCallback( window, windowPosCB );
  glfwSetWindowSizeCallback( window, windowSizeCB );
  glfwSetWindowCloseCallback( window, windowCloseCB );
  glfwSetWindowRefreshCallback( window, windowRefreshCB );
  glfwSetWindowFocusCallback( window, windowFocusCB );
  glfwSetWindowIconifyCallback( window, windowIconifyCB );
  glfwSetFramebufferSizeCallback( window, windowFramebufferSizeCB );

  // input callbacks
  glfwSetKeyCallback( window, keyCB);
  // TODO glfwSetCharCallback(window, TwEventCharGLFW);
  glfwSetMouseButtonCallback( window, mouseButtonCB );
  glfwSetCursorPosCallback( window, cursorPosCB );
  glfwSetCursorEnterCallback( window, cursorEnterCB );
  glfwSetScrollCallback( window, scrollCB );

  NanReturnValue(JS_NUM((uint64_t) window));
}

NAN_METHOD(DestroyWindow) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwDestroyWindow(window);
  }
  NanReturnUndefined();
}

NAN_METHOD(SetWindowTitle) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  String::Utf8Value str(args[1]->ToString());
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwSetWindowTitle(window, *str);
  }
  NanReturnUndefined();
}

NAN_METHOD(GetWindowSize) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    int w,h;
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwGetWindowSize(window, &w, &h);
    Local<Array> arr=Array::New(v8::Isolate::GetCurrent(),2);
    arr->Set(JS_STR("width"),JS_INT(w));
    arr->Set(JS_STR("height"),JS_INT(h));
    NanReturnValue(arr);
  }
  NanReturnUndefined();
}

NAN_METHOD(SetWindowSize) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwSetWindowSize(window, args[1]->Uint32Value(),args[2]->Uint32Value());
  }
  NanReturnUndefined();
}

NAN_METHOD(SetWindowPos) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwSetWindowPos(window, args[1]->Uint32Value(),args[2]->Uint32Value());
  }
  NanReturnUndefined();
}

NAN_METHOD(GetWindowPos) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    int xpos, ypos;
    glfwGetWindowPos(window, &xpos, &ypos);
    Local<Array> arr=Array::New(v8::Isolate::GetCurrent(),2);
    arr->Set(JS_STR("xpos"),JS_INT(xpos));
    arr->Set(JS_STR("ypos"),JS_INT(ypos));
    NanReturnValue(arr);
  }
  NanReturnUndefined();
}

NAN_METHOD(GetFramebufferSize) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    Local<Array> arr=Array::New(v8::Isolate::GetCurrent(),2);
    arr->Set(JS_STR("width"),JS_INT(width));
    arr->Set(JS_STR("height"),JS_INT(height));
    NanReturnValue(arr);
  }
  NanReturnUndefined();
}

NAN_METHOD(IconifyWindow) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwIconifyWindow(window);
  }
  NanReturnUndefined();
}

NAN_METHOD(RestoreWindow) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwRestoreWindow(window);
  }
  NanReturnUndefined();
}

NAN_METHOD(HideWindow) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwHideWindow(window);
  }
  NanReturnUndefined();
}

NAN_METHOD(ShowWindow) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwShowWindow(window);
  }
  NanReturnUndefined();
}

NAN_METHOD(WindowShouldClose) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    NanReturnValue(JS_INT(glfwWindowShouldClose(window)));
  }
  NanReturnUndefined();
}

NAN_METHOD(SetWindowShouldClose) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  int value=args[1]->Uint32Value();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwSetWindowShouldClose(window, value);
  }
  NanReturnUndefined();
}

NAN_METHOD(GetWindowAttrib) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  int attrib=args[1]->Uint32Value();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    NanReturnValue(JS_INT(glfwGetWindowAttrib(window, attrib)));
  }
  NanReturnUndefined();
}

NAN_METHOD(PollEvents) {
  NanScope();
  glfwPollEvents();
  NanReturnUndefined();
}

NAN_METHOD(WaitEvents) {
  NanScope();
  glfwWaitEvents();
  NanReturnUndefined();
}

/* Input handling */
NAN_METHOD(GetKey) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  int key=args[1]->Uint32Value();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    NanReturnValue(JS_INT(glfwGetKey(window, key)));
  }
  NanReturnUndefined();
}

NAN_METHOD(GetMouseButton) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  int button=args[1]->Uint32Value();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    NanReturnValue(JS_INT(glfwGetMouseButton(window, button)));
  }
  NanReturnUndefined();
}

NAN_METHOD(GetCursorPos) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    double x,y;
    glfwGetCursorPos(window, &x, &y);
    Local<Array> arr=Array::New(v8::Isolate::GetCurrent(),2);
    arr->Set(JS_STR("x"),JS_INT(x));
    arr->Set(JS_STR("y"),JS_INT(y));
    NanReturnValue(arr);
  }
  NanReturnUndefined();
}

NAN_METHOD(SetCursorPos) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  int x=args[1]->NumberValue();
  int y=args[2]->NumberValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwSetCursorPos(window, x, y);
  }
  NanReturnUndefined();
}

/* @Module Context handling */
NAN_METHOD(MakeContextCurrent) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwMakeContextCurrent(window);
  }
  NanReturnUndefined();
}

NAN_METHOD(GetCurrentContext) {
  NanScope();
  GLFWwindow* window=glfwGetCurrentContext();
  NanReturnValue(JS_NUM((uint64_t) window));
}

NAN_METHOD(SwapBuffers) {
  NanScope();
  uint64_t handle=args[0]->IntegerValue();
  if(handle) {
    GLFWwindow* window = reinterpret_cast<GLFWwindow*>(handle);
    glfwSwapBuffers(window);
  }
  NanReturnUndefined();
}

NAN_METHOD(SwapInterval) {
  NanScope();
  int interval=args[0]->Int32Value();
  glfwSwapInterval(interval);
  NanReturnUndefined();
}

/* Extension support */
NAN_METHOD(ExtensionSupported) {
  NanScope();
  String::Utf8Value str(args[0]->ToString());
  NanReturnValue(JS_BOOL(glfwExtensionSupported(*str)==1));
}

// make sure we close everything when we exit
void AtExit() {
  TwTerminate();
  glfwTerminate();
}

} // namespace glfw

///////////////////////////////////////////////////////////////////////////////
//
// bindings
//
///////////////////////////////////////////////////////////////////////////////
#define JS_GLFW_CONSTANT(name) target->Set(JS_STR( #name ), JS_INT(GLFW_ ## name))
#define JS_GLFW_SET_METHOD(name) NODE_SET_METHOD(target, #name , glfw::name);

extern "C" {
void init(Handle<Object> target) {
  atexit(glfw::AtExit);

  NanScope();

  /* GLFW initialization, termination and version querying */
  JS_GLFW_SET_METHOD(Init);
  JS_GLFW_SET_METHOD(Terminate);
  JS_GLFW_SET_METHOD(GetVersion);
  JS_GLFW_SET_METHOD(GetVersionString);

  /* Time */
  JS_GLFW_SET_METHOD(GetTime);
  JS_GLFW_SET_METHOD(SetTime);
  
  /* Monitor handling */
  JS_GLFW_SET_METHOD(GetMonitors);

  /* Window handling */
  //JS_GLFW_SET_METHOD(CreateWindow);
  NODE_SET_METHOD(target, "CreateWindow", glfw::glfw_CreateWindow);
  JS_GLFW_SET_METHOD(WindowHint);
  JS_GLFW_SET_METHOD(DefaultWindowHints);
  JS_GLFW_SET_METHOD(DestroyWindow);
  JS_GLFW_SET_METHOD(SetWindowShouldClose);
  JS_GLFW_SET_METHOD(WindowShouldClose);
  JS_GLFW_SET_METHOD(SetWindowTitle);
  JS_GLFW_SET_METHOD(GetWindowSize);
  JS_GLFW_SET_METHOD(SetWindowSize);
  JS_GLFW_SET_METHOD(SetWindowPos);
  JS_GLFW_SET_METHOD(GetWindowPos);
  JS_GLFW_SET_METHOD(GetFramebufferSize);
  JS_GLFW_SET_METHOD(IconifyWindow);
  JS_GLFW_SET_METHOD(RestoreWindow);
  JS_GLFW_SET_METHOD(ShowWindow);
  JS_GLFW_SET_METHOD(HideWindow);
  JS_GLFW_SET_METHOD(GetWindowAttrib);
  JS_GLFW_SET_METHOD(PollEvents);
  JS_GLFW_SET_METHOD(WaitEvents);

  /* Input handling */
  JS_GLFW_SET_METHOD(GetKey);
  JS_GLFW_SET_METHOD(GetMouseButton);
  JS_GLFW_SET_METHOD(GetCursorPos);
  JS_GLFW_SET_METHOD(SetCursorPos);

  /* Context handling */
  JS_GLFW_SET_METHOD(MakeContextCurrent);
  JS_GLFW_SET_METHOD(GetCurrentContext);
  JS_GLFW_SET_METHOD(SwapBuffers);
  JS_GLFW_SET_METHOD(SwapInterval);
  JS_GLFW_SET_METHOD(ExtensionSupported);

  /* Joystick */
  JS_GLFW_SET_METHOD(JoystickPresent);
  JS_GLFW_SET_METHOD(GetJoystickAxes);
  JS_GLFW_SET_METHOD(GetJoystickButtons);
  JS_GLFW_SET_METHOD(GetJoystickName);

  /*************************************************************************
   * GLFW version
   *************************************************************************/

  JS_GLFW_CONSTANT(VERSION_MAJOR);
  JS_GLFW_CONSTANT(VERSION_MINOR);
  JS_GLFW_CONSTANT(VERSION_REVISION);

  /*************************************************************************
   * Input handling definitions
   *************************************************************************/

  /* Key and button state/action definitions */
  JS_GLFW_CONSTANT(RELEASE);
  JS_GLFW_CONSTANT(PRESS);
  JS_GLFW_CONSTANT(REPEAT);

  /* These key codes are inspired by the *USB HID Usage Tables v1.12* (p. 53-60),
   * but re-arranged to map to 7-bit ASCII for printable keys (function keys are
   * put in the 256+ range).
   *
   * The naming of the key codes follow these rules:
   *  - The US keyboard layout is used
   *  - Names of printable alpha-numeric characters are used (e.g. "A", "R",
   *    "3", etc.)
   *  - For non-alphanumeric characters, Unicode:ish names are used (e.g.
   *    "COMMA", "LEFT_SQUARE_BRACKET", etc.). Note that some names do not
   *    correspond to the Unicode standard (usually for brevity)
   *  - Keys that lack a clear US mapping are named "WORLD_x"
   *  - For non-printable keys, custom names are used (e.g. "F4",
   *    "BACKSPACE", etc.)
   */

  /* The unknown key */
  JS_GLFW_CONSTANT(KEY_UNKNOWN);

  /* Printable keys */
  JS_GLFW_CONSTANT(KEY_SPACE);
  JS_GLFW_CONSTANT(KEY_APOSTROPHE);
  JS_GLFW_CONSTANT(KEY_COMMA);
  JS_GLFW_CONSTANT(KEY_MINUS);
  JS_GLFW_CONSTANT(KEY_PERIOD);
  JS_GLFW_CONSTANT(KEY_SLASH);
  JS_GLFW_CONSTANT(KEY_0);
  JS_GLFW_CONSTANT(KEY_1);
  JS_GLFW_CONSTANT(KEY_2);
  JS_GLFW_CONSTANT(KEY_3);
  JS_GLFW_CONSTANT(KEY_4);
  JS_GLFW_CONSTANT(KEY_5);
  JS_GLFW_CONSTANT(KEY_6);
  JS_GLFW_CONSTANT(KEY_7);
  JS_GLFW_CONSTANT(KEY_8);
  JS_GLFW_CONSTANT(KEY_9);
  JS_GLFW_CONSTANT(KEY_SEMICOLON);
  JS_GLFW_CONSTANT(KEY_EQUAL);
  JS_GLFW_CONSTANT(KEY_A);
  JS_GLFW_CONSTANT(KEY_B);
  JS_GLFW_CONSTANT(KEY_C);
  JS_GLFW_CONSTANT(KEY_D);
  JS_GLFW_CONSTANT(KEY_E);
  JS_GLFW_CONSTANT(KEY_F);
  JS_GLFW_CONSTANT(KEY_G);
  JS_GLFW_CONSTANT(KEY_H);
  JS_GLFW_CONSTANT(KEY_I);
  JS_GLFW_CONSTANT(KEY_J);
  JS_GLFW_CONSTANT(KEY_K);
  JS_GLFW_CONSTANT(KEY_L);
  JS_GLFW_CONSTANT(KEY_M);
  JS_GLFW_CONSTANT(KEY_N);
  JS_GLFW_CONSTANT(KEY_O);
  JS_GLFW_CONSTANT(KEY_P);
  JS_GLFW_CONSTANT(KEY_Q);
  JS_GLFW_CONSTANT(KEY_R);
  JS_GLFW_CONSTANT(KEY_S);
  JS_GLFW_CONSTANT(KEY_T);
  JS_GLFW_CONSTANT(KEY_U);
  JS_GLFW_CONSTANT(KEY_V);
  JS_GLFW_CONSTANT(KEY_W);
  JS_GLFW_CONSTANT(KEY_X);
  JS_GLFW_CONSTANT(KEY_Y);
  JS_GLFW_CONSTANT(KEY_Z);
  JS_GLFW_CONSTANT(KEY_LEFT_BRACKET);
  JS_GLFW_CONSTANT(KEY_BACKSLASH);
  JS_GLFW_CONSTANT(KEY_RIGHT_BRACKET);
  JS_GLFW_CONSTANT(KEY_GRAVE_ACCENT);
  JS_GLFW_CONSTANT(KEY_WORLD_1);
  JS_GLFW_CONSTANT(KEY_WORLD_2);

  /* Function keys */
  JS_GLFW_CONSTANT(KEY_ESCAPE);
  JS_GLFW_CONSTANT(KEY_ENTER);
  JS_GLFW_CONSTANT(KEY_TAB);
  JS_GLFW_CONSTANT(KEY_BACKSPACE);
  JS_GLFW_CONSTANT(KEY_INSERT);
  JS_GLFW_CONSTANT(KEY_DELETE);
  JS_GLFW_CONSTANT(KEY_RIGHT);
  JS_GLFW_CONSTANT(KEY_LEFT);
  JS_GLFW_CONSTANT(KEY_DOWN);
  JS_GLFW_CONSTANT(KEY_UP);
  JS_GLFW_CONSTANT(KEY_PAGE_UP);
  JS_GLFW_CONSTANT(KEY_PAGE_DOWN);
  JS_GLFW_CONSTANT(KEY_HOME);
  JS_GLFW_CONSTANT(KEY_END);
  JS_GLFW_CONSTANT(KEY_CAPS_LOCK);
  JS_GLFW_CONSTANT(KEY_SCROLL_LOCK);
  JS_GLFW_CONSTANT(KEY_NUM_LOCK);
  JS_GLFW_CONSTANT(KEY_PRINT_SCREEN);
  JS_GLFW_CONSTANT(KEY_PAUSE);
  JS_GLFW_CONSTANT(KEY_F1);
  JS_GLFW_CONSTANT(KEY_F2);
  JS_GLFW_CONSTANT(KEY_F3);
  JS_GLFW_CONSTANT(KEY_F4);
  JS_GLFW_CONSTANT(KEY_F5);
  JS_GLFW_CONSTANT(KEY_F6);
  JS_GLFW_CONSTANT(KEY_F7);
  JS_GLFW_CONSTANT(KEY_F8);
  JS_GLFW_CONSTANT(KEY_F9);
  JS_GLFW_CONSTANT(KEY_F10);
  JS_GLFW_CONSTANT(KEY_F11);
  JS_GLFW_CONSTANT(KEY_F12);
  JS_GLFW_CONSTANT(KEY_F13);
  JS_GLFW_CONSTANT(KEY_F14);
  JS_GLFW_CONSTANT(KEY_F15);
  JS_GLFW_CONSTANT(KEY_F16);
  JS_GLFW_CONSTANT(KEY_F17);
  JS_GLFW_CONSTANT(KEY_F18);
  JS_GLFW_CONSTANT(KEY_F19);
  JS_GLFW_CONSTANT(KEY_F20);
  JS_GLFW_CONSTANT(KEY_F21);
  JS_GLFW_CONSTANT(KEY_F22);
  JS_GLFW_CONSTANT(KEY_F23);
  JS_GLFW_CONSTANT(KEY_F24);
  JS_GLFW_CONSTANT(KEY_F25);
  JS_GLFW_CONSTANT(KEY_KP_0);
  JS_GLFW_CONSTANT(KEY_KP_1);
  JS_GLFW_CONSTANT(KEY_KP_2);
  JS_GLFW_CONSTANT(KEY_KP_3);
  JS_GLFW_CONSTANT(KEY_KP_4);
  JS_GLFW_CONSTANT(KEY_KP_5);
  JS_GLFW_CONSTANT(KEY_KP_6);
  JS_GLFW_CONSTANT(KEY_KP_7);
  JS_GLFW_CONSTANT(KEY_KP_8);
  JS_GLFW_CONSTANT(KEY_KP_9);
  JS_GLFW_CONSTANT(KEY_KP_DECIMAL);
  JS_GLFW_CONSTANT(KEY_KP_DIVIDE);
  JS_GLFW_CONSTANT(KEY_KP_MULTIPLY);
  JS_GLFW_CONSTANT(KEY_KP_SUBTRACT);
  JS_GLFW_CONSTANT(KEY_KP_ADD);
  JS_GLFW_CONSTANT(KEY_KP_ENTER);
  JS_GLFW_CONSTANT(KEY_KP_EQUAL);
  JS_GLFW_CONSTANT(KEY_LEFT_SHIFT);
  JS_GLFW_CONSTANT(KEY_LEFT_CONTROL);
  JS_GLFW_CONSTANT(KEY_LEFT_ALT);
  JS_GLFW_CONSTANT(KEY_LEFT_SUPER);
  JS_GLFW_CONSTANT(KEY_RIGHT_SHIFT);
  JS_GLFW_CONSTANT(KEY_RIGHT_CONTROL);
  JS_GLFW_CONSTANT(KEY_RIGHT_ALT);
  JS_GLFW_CONSTANT(KEY_RIGHT_SUPER);
  JS_GLFW_CONSTANT(KEY_MENU);
  JS_GLFW_CONSTANT(KEY_LAST);

  /*Modifier key flags*/

  /*If this bit is set one or more Shift keys were held down. */
  JS_GLFW_CONSTANT(MOD_SHIFT);
  /*If this bit is set one or more Control keys were held down. */
  JS_GLFW_CONSTANT(MOD_CONTROL);
  /*If this bit is set one or more Alt keys were held down. */
  JS_GLFW_CONSTANT(MOD_ALT);
  /*If this bit is set one or more Super keys were held down. */
  JS_GLFW_CONSTANT(MOD_SUPER);

  /*Mouse buttons*/
  JS_GLFW_CONSTANT(MOUSE_BUTTON_1);
  JS_GLFW_CONSTANT(MOUSE_BUTTON_2);
  JS_GLFW_CONSTANT(MOUSE_BUTTON_3);
  JS_GLFW_CONSTANT(MOUSE_BUTTON_4);
  JS_GLFW_CONSTANT(MOUSE_BUTTON_5);
  JS_GLFW_CONSTANT(MOUSE_BUTTON_6);
  JS_GLFW_CONSTANT(MOUSE_BUTTON_7);
  JS_GLFW_CONSTANT(MOUSE_BUTTON_8);
  JS_GLFW_CONSTANT(MOUSE_BUTTON_LAST);
  JS_GLFW_CONSTANT(MOUSE_BUTTON_LEFT);
  JS_GLFW_CONSTANT(MOUSE_BUTTON_RIGHT);
  JS_GLFW_CONSTANT(MOUSE_BUTTON_MIDDLE);

  /*Joysticks*/
  JS_GLFW_CONSTANT(JOYSTICK_1);
  JS_GLFW_CONSTANT(JOYSTICK_2);
  JS_GLFW_CONSTANT(JOYSTICK_3);
  JS_GLFW_CONSTANT(JOYSTICK_4);
  JS_GLFW_CONSTANT(JOYSTICK_5);
  JS_GLFW_CONSTANT(JOYSTICK_6);
  JS_GLFW_CONSTANT(JOYSTICK_7);
  JS_GLFW_CONSTANT(JOYSTICK_8);
  JS_GLFW_CONSTANT(JOYSTICK_9);
  JS_GLFW_CONSTANT(JOYSTICK_10);
  JS_GLFW_CONSTANT(JOYSTICK_11);
  JS_GLFW_CONSTANT(JOYSTICK_12);
  JS_GLFW_CONSTANT(JOYSTICK_13);
  JS_GLFW_CONSTANT(JOYSTICK_14);
  JS_GLFW_CONSTANT(JOYSTICK_15);
  JS_GLFW_CONSTANT(JOYSTICK_16);
  JS_GLFW_CONSTANT(JOYSTICK_LAST);

  /*errors Error codes*/

  /*GLFW has not been initialized.*/
  JS_GLFW_CONSTANT(NOT_INITIALIZED);
  /*No context is current for this thread.*/
  JS_GLFW_CONSTANT(NO_CURRENT_CONTEXT);
  /*One of the enum parameters for the function was given an invalid enum.*/
  JS_GLFW_CONSTANT(INVALID_ENUM);
  /*One of the parameters for the function was given an invalid value.*/
  JS_GLFW_CONSTANT(INVALID_VALUE);
  /*A memory allocation failed.*/
  JS_GLFW_CONSTANT(OUT_OF_MEMORY);
  /*GLFW could not find support for the requested client API on the system.*/
  JS_GLFW_CONSTANT(API_UNAVAILABLE);
  /*The requested client API version is not available.*/
  JS_GLFW_CONSTANT(VERSION_UNAVAILABLE);
  /*A platform-specific error occurred that does not match any of the more specific categories.*/
  JS_GLFW_CONSTANT(PLATFORM_ERROR);
  /*The clipboard did not contain data in the requested format.*/
  JS_GLFW_CONSTANT(FORMAT_UNAVAILABLE);

  JS_GLFW_CONSTANT(FOCUSED);
  JS_GLFW_CONSTANT(ICONIFIED);
  JS_GLFW_CONSTANT(RESIZABLE);
  JS_GLFW_CONSTANT(VISIBLE);
  JS_GLFW_CONSTANT(DECORATED);

  JS_GLFW_CONSTANT(RED_BITS);
  JS_GLFW_CONSTANT(GREEN_BITS);
  JS_GLFW_CONSTANT(BLUE_BITS);
  JS_GLFW_CONSTANT(ALPHA_BITS);
  JS_GLFW_CONSTANT(DEPTH_BITS);
  JS_GLFW_CONSTANT(STENCIL_BITS);
  JS_GLFW_CONSTANT(ACCUM_RED_BITS);
  JS_GLFW_CONSTANT(ACCUM_GREEN_BITS);
  JS_GLFW_CONSTANT(ACCUM_BLUE_BITS);
  JS_GLFW_CONSTANT(ACCUM_ALPHA_BITS);
  JS_GLFW_CONSTANT(AUX_BUFFERS);
  JS_GLFW_CONSTANT(STEREO);
  JS_GLFW_CONSTANT(SAMPLES);
  JS_GLFW_CONSTANT(SRGB_CAPABLE);
  JS_GLFW_CONSTANT(REFRESH_RATE);

  JS_GLFW_CONSTANT(CLIENT_API);
  JS_GLFW_CONSTANT(CONTEXT_VERSION_MAJOR);
  JS_GLFW_CONSTANT(CONTEXT_VERSION_MINOR);
  JS_GLFW_CONSTANT(CONTEXT_REVISION);
  JS_GLFW_CONSTANT(CONTEXT_ROBUSTNESS);
  JS_GLFW_CONSTANT(OPENGL_FORWARD_COMPAT);
  JS_GLFW_CONSTANT(OPENGL_DEBUG_CONTEXT);
  JS_GLFW_CONSTANT(OPENGL_PROFILE);

  JS_GLFW_CONSTANT(OPENGL_API);
  JS_GLFW_CONSTANT(OPENGL_ES_API);

  JS_GLFW_CONSTANT(NO_ROBUSTNESS);
  JS_GLFW_CONSTANT(NO_RESET_NOTIFICATION);
  JS_GLFW_CONSTANT(LOSE_CONTEXT_ON_RESET);

  JS_GLFW_CONSTANT(OPENGL_ANY_PROFILE);
  JS_GLFW_CONSTANT(OPENGL_CORE_PROFILE);
  JS_GLFW_CONSTANT(OPENGL_COMPAT_PROFILE);

  JS_GLFW_CONSTANT(CURSOR);
  JS_GLFW_CONSTANT(STICKY_KEYS);
  JS_GLFW_CONSTANT(STICKY_MOUSE_BUTTONS);

  JS_GLFW_CONSTANT(CURSOR_NORMAL);
  JS_GLFW_CONSTANT(CURSOR_HIDDEN);
  JS_GLFW_CONSTANT(CURSOR_DISABLED);

  JS_GLFW_CONSTANT(CONNECTED);
  JS_GLFW_CONSTANT(DISCONNECTED);

  // init AntTweakBar
  atb::AntTweakBar::Initialize(target);
  atb::Bar::Initialize(target);

  // test scene
  JS_GLFW_SET_METHOD(testScene);
  JS_GLFW_SET_METHOD(testJoystick);
}

NODE_MODULE(glfw, init)
}

