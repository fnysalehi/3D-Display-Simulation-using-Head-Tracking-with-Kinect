#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GL/glut.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <math.h>
#include <sstream>
#include <iostream>

#include "kinect/Tracker.h"

Tracker* tracker = nullptr;

int mouseoldx, mouseoldy ; // For mouse motion
GLdouble eyeloc = 2.0 ; // Where to look from; initially 0 -2, 2
glm::vec3 up = glm::vec3(0, 1, 1);
glm::vec3 center = glm::vec3(0, 0, 0);
GLfloat teapotloc = -0.5 ; // where the teapot is located
GLint animate = 0 ; // whether to animate or not
GLuint vertexshader, fragmentshader, shaderprogram ; // shaders
GLuint projectionPos, modelviewPos, colorPos; // Locations of uniform variables
glm::mat4 projection, modelview; // The mvp matrices themselves
glm::mat4 identity(1.0f); // An identity matrix used for making transformation matrices

GLubyte woodtexture[256][256][3] ; //texture (from grsites.com)
GLuint texNames[1] ; // texture buffer
GLuint istex ;  // blend parameter for texturing
GLuint islight ; // for lighting
GLint texturing = 1 ; // to turn on/off texturing
GLint lighting = 1 ; // to turn on/off lighting
GLint amountx = 0;
GLint amounty = 0;
GLdouble amountHead = 0;
GLdouble amountCenter = 0;

/* Variables to set uniform params for lighting fragment shader */
GLuint light0dirn ; 
GLuint light0color ; 
GLuint light1posn ; 
GLuint light1color ; 
GLuint ambient ; 
GLuint diffuse ; 
GLuint specular ; 
GLuint shininess ; 


const int DEMO = 5 ; // To turn on and off features

#include "shaders.h"
#include "geometry3.h"

/* New helper transformation function to transform vector by modelview */ 
void transformvec (const GLfloat input[4], GLfloat output[4]) {
	glm::vec4 inputvec(input[0], input[1], input[2], input[3]);
	glm::vec4 outputvec = modelview * inputvec;
	output[0] = outputvec[0];
	output[1] = outputvec[1];
	output[2] = outputvec[2];
	output[3] = outputvec[3];
}

// Treat this as a destructor function. Delete any dynamically allocated memory here
void deleteBuffers() {
	glDeleteVertexArrays(numobjects + ncolors, VAOs);
	glDeleteVertexArrays(1, &teapotVAO);
	glDeleteBuffers(numperobj*numobjects + ncolors, buffers);
	glDeleteBuffers(3, teapotbuffers);
}

void display(void)
{
  // Clear all pixels in the buffer

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) ; 

  // draw white polygon (square) of unit length centered at the origin
  // Note that vertices must generally go counterclockwise
  // Change from the first program, in that I just made it white.

  // Does the order of drawing matter?  What happens if I draw the ground 
  // after the pillars?  I will show this in class 

  glUniform1i(islight,0) ; // Turn off lighting (except on teapot, later)
  glUniform1i(istex,texturing) ;

  // Draw the floor
  // Start with no modifications made to the model matrix
  glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(modelview)[0][0]);
  glUniform3f(colorPos, 1.0f, 1.0f, 1.0f); // The floor is white
  drawtexture(FLOOR,texNames[0]) ; // Texturing floor 
  glUniform1i(istex,0) ; // Other items aren't textured 

  // Now draw several cubes with different transforms, colors

  // We now maintain a stack for the modelview matrices. Changes made to the stack after pushing
  // are discarded once it is popped.
  pushMatrix(modelview);
  // 1st pillar 
  // glm functions build a new matrix. They don't actually modify the passed in matrix.
  // Consequently, we need to assign this result to modelview.
  modelview = modelview * glm::translate(identity, glm::vec3(-0.4, -0.4, 0.0));
  glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(modelview)[0][0]);
  glUniform3fv(colorPos, 1, _cubecol[0]);
  drawcolor(CUBE, 0);
  popMatrix(modelview);

  // 2nd pillar 
  pushMatrix(modelview);
  modelview = modelview * glm::translate(identity, glm::vec3(0.4, -0.4, 0.0));
  glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(modelview)[0][0]);
  glUniform3fv(colorPos, 1, _cubecol[1]);
  drawcolor(CUBE, 1);
  popMatrix(modelview);

  // 3rd pillar 
  pushMatrix(modelview);
  modelview = modelview * glm::translate(identity, glm::vec3(0.4, 0.4, 0.0));
  glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(modelview)[0][0]);
  glUniform3fv(colorPos, 1, _cubecol[2]);
  drawcolor(CUBE, 2);
  popMatrix(modelview);

  // 4th pillar 
  pushMatrix(modelview);
  modelview = modelview * glm::translate(identity, glm::vec3(-0.4, 0.4, 0.0));
  glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(modelview)[0][0]);
  glUniform3fv(colorPos, 1, _cubecol[3]);
  drawcolor(CUBE, 3);
  popMatrix(modelview);

  // Draw the glut teapot 

  /* New for Demo 3; add lighting effects */ 
  {
    const GLfloat one[] = {1, 1, 1, 1};
    const GLfloat medium[] = {0.5f, 0.5f, 0.5f, 1};
    const GLfloat smal[] = {0.2f, 0.2f, 0.2f, 1};
    const GLfloat high[] = {100} ;
    const GLfloat zero[] = {0.0, 0.0, 0.0, 1.0} ; 
    const GLfloat light_specular[] = {1, 0.5, 0, 1};
    const GLfloat light_specular1[] = {0, 0.5, 1, 1};
    const GLfloat light_direction[] = {0.5, 0, 0, 0}; // Dir light 0 in w 
    const GLfloat light_position1[] = {0, -0.5, 0, 1};

    GLfloat light0[4], light1[4] ; 

    // Set Light and Material properties for the teapot
    // Lights are transformed by current modelview matrix. 
    // The shader can't do this globally. 
    // So we need to do so manually.  
    transformvec(light_direction, light0) ; 
    transformvec(light_position1, light1) ; 

    glUniform3fv(light0dirn, 1, light0) ; 
    glUniform4fv(light0color, 1, light_specular) ; 
    glUniform4fv(light1posn, 1, light1) ; 
    glUniform4fv(light1color, 1, light_specular1) ; 
    // glUniform4fv(light1color, 1, zero) ; 

    glUniform4fv(ambient,1,smal) ; 
    glUniform4fv(diffuse,1,medium) ; 
    glUniform4fv(specular,1,one) ; 
    glUniform1fv(shininess,1,high) ; 

    // Enable and Disable everything around the teapot 
    // Generally, we would also need to define normals etc. 
    // But in old OpenGL, GLUT already does this for us. In modern OpenGL, the 
	// 3D model file for the teapot also defines normals already.
    if (DEMO > 4) 
      glUniform1i(islight,lighting) ; // turn on lighting only for teapot. 

  }
	// Put a teapot in the middle that animates 
	glUniform3f(colorPos, 0.0f, 1.0f, 1.0f);
	// Put a teapot in the middle that animates
	pushMatrix(modelview);
	modelview = modelview * glm::translate(identity, glm::vec3(teapotloc, 0.0, 0.0));

	//  The following two transforms set up and center the teapot 
	//  Remember that transforms right-multiply the modelview matrix (top of the stack)
	modelview = modelview * glm::translate(identity, glm::vec3(0.0, 0.0, 0.1));
	modelview = modelview * glm::rotate(identity, glm::pi<float>() / 2.0f, glm::vec3(1.0, 0.0, 0.0));
	float size = 0.235f; // Teapot size
	modelview = modelview * glm::scale(identity, glm::vec3(size, size, size));
	glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &(modelview)[0][0]);
    drawteapot() ;
	popMatrix(modelview);


  glutSwapBuffers() ; 
  glFlush ();
}

void animation(void) {
  teapotloc = teapotloc * 0.005 ;
  if (teapotloc > 0.5) teapotloc = -0.5 ;
  glutPostRedisplay() ;  
}

      
void mouse(int button, int state, int x, int y) 
{
  if (button == GLUT_LEFT_BUTTON) {
    if (state == GLUT_UP) {
      // Do Nothing ;
    }
    else if (state == GLUT_DOWN) {
      mouseoldx = x ; mouseoldy = y ; // so we can move wrt x , y 
    }
  }
  else if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) 
  {
	  // Reset lookAt
	  eyeloc = 2.0;
	  up = glm::vec3(0, 0, 1);
	  amountx = 0;
	  amounty = 0;
	  amountHead = 0;
	  modelview = glm::lookAt(glm::vec3(0, -eyeloc, eyeloc), glm::vec3(0, 0, 0), glm::vec3(0, 1, 1));
	  // Send the updated matrix over to the shader
	  glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &modelview[0][0]);
	  glutPostRedisplay();
  }
}

void mousedrag(int x, int y) {
  int yloc = y - mouseoldy  ;    // We will use the y coord to zoom in/out
  eyeloc  += 0.005*yloc ;         // Where do we look from
  if (eyeloc < 0) eyeloc = 0.0 ;
  mouseoldy = y ;

  /* Set the eye location */
  modelview = glm::lookAt(glm::vec3(-amountHead, -eyeloc, eyeloc), glm::vec3(-amountHead, 0, 0), glm::vec3(0, 1, 1));
  // Send the updated matrix over to the shader
  glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &modelview[0][0]);

  glutPostRedisplay() ;
}

glm::mat3 rotate(const float degrees, const glm::vec3& axis) {
	float degree = glm::radians(degrees);
	glm::mat3 temp = glm::outerProduct(axis, axis);
	glm::mat3 dual = glm::transpose(glm::mat3(0.0, -axis[2], axis[1], axis[2], 0.0, -axis[0], -axis[1], axis[0], 0.0));
	glm::mat3 ans = (cos(degree) * glm::mat3(1.0)) + ((1 - cos(degree)) * temp) + (glm::sin(degree)* dual);

	// You will change this return call
	return ans;
}

// Transforms the camera left around the "crystal ball" interface
glm::vec3 left(float degrees, glm::vec3& eye, glm::vec3& up) {
	eye = eye * rotate(degrees, glm::normalize(up));
	return eye;
}

glm::vec3 mrotate(float degreesY, float degreesX, glm::vec3& eye, glm::vec3& up) {
	glm::vec3 axis = glm::cross(eye, up);
	axis = glm::normalize(axis);
	eye = eye * rotate(degreesX, glm::normalize(up));
	//up = rotate(degreesY, axis) * up;
	eye = rotate(degreesY, axis)*eye;
	return eye;
}

void moveEye() {
	center[0] += amountCenter;
	modelview = glm::lookAt(glm::vec3(-amountHead, -eyeloc, eyeloc), center, up);
	printf("e eye:  %f\t%f\t%f\n", -amountHead, -eyeloc, eyeloc);
	printf("e center:  %f\t%f\t%f\n", center[0], center[1], center[2]);
	printf("e center:  %f\t%f\t%f\n", up[0], up[1], up[2]);
}

void moveCamera()
{
	//glm::vec3 up = glm::vec3(0, 1, 1);
	glm::vec3 u = glm::cross(up, glm::vec3(0, -eyeloc, eyeloc));
	glm::normalize(u);
	glm::vec3 new_center = mrotate(amounty, amountx, glm::vec3(0,eyeloc,-eyeloc), up);
	//new_center = left(amountx, glm::vec3(0, eyeloc, -eyeloc), u);
	center = new_center + glm::vec3(-amountHead, -eyeloc, eyeloc);
	if (abs(glm::dot(glm::vec3(0, -eyeloc, eyeloc), up)) >= 0.985) {
		up = mrotate(amounty, amountx, up, u);
	}
	modelview = glm::lookAt(glm::vec3(-amountHead, -eyeloc, eyeloc), center, up);
	printf("c eye:  %f\t%f\t%f\n", -amountHead, -eyeloc, eyeloc);
	printf("c center:  %f\t%f\t%f\n", center[0], center[1], center[2]);
	printf("c center:  %f\t%f\t%f\n", up[0], up[1], up[2]);
	// Send the updated matrix over to the shader
	//glUniformMatrix4fv(modelviewPos, 1, GL_FALSE, &modelview[0][0]);
}
// Defines what to do when various keys are pressed 
void keyboard (unsigned char key, int x, int y) 
{
  switch (key) {
    case 27:  // Escape to quit
      exit(0) ;
      break ;
    case 'p': // to pause/restart animation
      animate = !animate ;
      if (animate) glutIdleFunc(animation) ;
      else glutIdleFunc(NULL) ;
      break ;
    case 't': // to turn on/off texturing ; 
      texturing = !texturing ;
      glutPostRedisplay() ; 
      break ;
    case 's': // to turn on/off shading (always smooth) ; 
      lighting = !lighting ;
      glutPostRedisplay() ; 
      break ;
	case 'b': //move camera around scene left;
		amountx -= 1;
		moveCamera();
		glutPostRedisplay();
		break;
	case 'n': //move camera around scene right;
		amountx += 1;
		moveCamera();
		glutPostRedisplay();
		break;
	case 'j': //move camera around scene up;
		amounty += 1;
		moveCamera();
		glutPostRedisplay();
		break;
	case 'h': //move camera around scene down;
		amounty -= 1;
		moveCamera();
		glutPostRedisplay();
		break;
	case 'w': //move head right;
		amountHead += 0.05;
		amountCenter = -0.05;
		moveEye();
		glutPostRedisplay();
		break;
	case 'e': //move head right;
		amountHead -= 0.05;
		amountCenter = 0.05;
		moveEye();
		glutPostRedisplay();
		break;
    default:
      break ;
  }
}

/* Reshapes the window appropriately */
void reshape(int w, int h)
{
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);

	projection = glm::perspective(30.0f / 180.0f * glm::pi<float>(), (GLfloat)w / (GLfloat)h, 1.0f, 10.0f);
	glUniformMatrix4fv(projectionPos, 1, GL_FALSE, &projection[0][0]);

}


void init (void) 
{
  /* select clearing color 	*/
  glClearColor (0.0, 0.0, 0.0, 0.0);

  /* initialize viewing values  */
  projection = glm::mat4(1.0f); // The identity matrix
  modelview = glm::lookAt(glm::vec3(0, -eyeloc, eyeloc), glm::vec3(0, 0, 0), glm::vec3(0, 1, 1));


  // Initialize the shaders
  vertexshader = initshaders(GL_VERTEX_SHADER, "shaders/light.vert") ;
  fragmentshader = initshaders(GL_FRAGMENT_SHADER, "shaders/light.frag") ;
  shaderprogram = initprogram(vertexshader, fragmentshader) ; 

  // * NEW * Set up the shader parameter mappings properly for lighting.
  islight = glGetUniformLocation(shaderprogram,"islight") ;        
  light0dirn = glGetUniformLocation(shaderprogram,"light0dirn") ;       
  light0color = glGetUniformLocation(shaderprogram,"light0color") ;       
  light1posn = glGetUniformLocation(shaderprogram,"light1posn") ;       
  light1color = glGetUniformLocation(shaderprogram,"light1color") ;       
  ambient = glGetUniformLocation(shaderprogram,"ambient") ;       
  diffuse = glGetUniformLocation(shaderprogram,"diffuse") ;       
  specular = glGetUniformLocation(shaderprogram,"specular") ;       
  shininess = glGetUniformLocation(shaderprogram,"shininess") ;

  // Get the positions of other uniform variables
  projectionPos = glGetUniformLocation(shaderprogram, "projection");
  modelviewPos = glGetUniformLocation(shaderprogram, "modelview");
  colorPos = glGetUniformLocation(shaderprogram, "color");

  // Now create the buffer objects to be used in the scene later
  glGenVertexArrays(numobjects + ncolors, VAOs);
  glGenVertexArrays(1, &teapotVAO);
  glGenBuffers(numperobj * numobjects + ncolors + 1, buffers); // 1 extra buffer for the texcoords
  glGenBuffers(3, teapotbuffers);

  // Initialize texture
  inittexture("wood.ppm", shaderprogram) ; 

  // Initialize objects
  initobject(FLOOR, (GLfloat *) floorverts, sizeof(floorverts), (GLfloat *) floorcol, sizeof (floorcol), (GLubyte *) floorinds, sizeof (floorinds), GL_TRIANGLES) ; 
  initcubes(CUBE, (GLfloat *)cubeverts, sizeof(cubeverts), (GLubyte *)cubeinds, sizeof(cubeinds), GL_TRIANGLES);
  loadteapot();

  // Enable the depth test
  glEnable(GL_DEPTH_TEST) ;
  glDepthFunc (GL_LESS) ; // The default option
}

int main(int argc, char** argv)
{
  glutInit(&argc, argv);

  // Requests the type of buffers (Single, RGB).
  // Think about what buffers you would need...

 
  glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);

  glutInitWindowSize (500, 500); 
  glutInitWindowPosition (100, 100);
  glutCreateWindow ("Simple Demo with Shaders");

  GLenum err = glewInit(); 
  if (GLEW_OK != err) {
    std::cerr << "Error: " << glewGetString(err) << std::endl;
  }

  tracker = new Tracker();
  tracker->Init();

  // TODO: inja ro bekhoon hoy
  // har frame tracker->Update() ro call mikoni
  // baad check mikoni ke tracker->LastTrackSucceeded() ya na
  // age are ba tracker->GetResult() natijeye tracking ro migiri o bar asasesh tekoon midi

  init (); // Always initialize first

  // Now, we define callbacks and functions for various tasks.
  glutDisplayFunc(display); 
  glutReshapeFunc(reshape) ;
  glutKeyboardFunc(keyboard);
  glutMouseFunc(mouse) ;
  glutMotionFunc(mousedrag) ;

  glutMainLoop(); // Start the main code
  deleteBuffers(); // Program terminate. Destroy dynamically allocated memory.

  tracker->Destroy();
  delete tracker;

  return 0;   /* ANSI C requires main to return int. */
}