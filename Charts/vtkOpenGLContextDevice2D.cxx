/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkOpenGLContextDevice2D.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkOpenGLContextDevice2D.h"

#ifdef VTK_USE_QT
# include <QApplication>
# include "vtkQtStringToImage.h"
#endif
#include "vtkFreeTypeStringToImage.h"

#include "vtkVector.h"
#include "vtkPen.h"
#include "vtkBrush.h"
#include "vtkTextProperty.h"
#include "vtkPoints2D.h"
#include "vtkMatrix3x3.h"
#include "vtkFloatArray.h"
#include "vtkSmartPointer.h"

#include "vtkMath.h"
#include "vtkObjectFactory.h"

#include "vtkViewport.h"
#include "vtkWindow.h"

#include "vtkTexture.h"
#include "vtkImageData.h"

#include "vtkRenderer.h"
#include "vtkOpenGLRenderer.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLExtensionManager.h"
#include "vtkShaderProgram2.h"
#include "vtkgl.h"

#include "vtkObjectFactory.h"
#include "vtkContextBufferId.h"
#include "vtkOpenGLContextBufferId.h"

//-----------------------------------------------------------------------------
class vtkOpenGLContextDevice2D::Private
{
public:
  Private()
  {
    this->Texture = NULL;
    this->TextureProperties = vtkContextDevice2D::Linear |
        vtkContextDevice2D::Stretch;
    this->SpriteTexture = NULL;
    this->SavedLighting = GL_TRUE;
    this->SavedDepthTest = GL_TRUE;
    this->SavedAlphaTest = GL_TRUE;
    this->SavedStencilTest = GL_TRUE;
    this->SavedBlend = GL_TRUE;
    this->SavedDrawBuffer = 0;
    this->SavedClearColor[0] = this->SavedClearColor[1] =
                               this->SavedClearColor[2] =
                               this->SavedClearColor[3] = 0.0f;
    this->TextCounter = 0;
    this->GLExtensionsLoaded = false;
    this->OpenGL15 = false;
    this->OpenGL20 = false;
    this->GLSL = false;
  }

  ~Private()
  {
    if (this->Texture)
      {
      this->Texture->Delete();
      this->Texture = NULL;
      }
    if (this->SpriteTexture)
      {
      this->SpriteTexture->Delete();
      this->SpriteTexture = NULL;
      }
  }

  void SaveGLState(bool colorBuffer = false)
  {
    this->SavedLighting = glIsEnabled(GL_LIGHTING);
    this->SavedDepthTest = glIsEnabled(GL_DEPTH_TEST);

    if (colorBuffer)
      {
      this->SavedAlphaTest = glIsEnabled(GL_ALPHA_TEST);
      this->SavedStencilTest = glIsEnabled(GL_STENCIL_TEST);
      this->SavedBlend = glIsEnabled(GL_BLEND);
      glGetFloatv(GL_COLOR_CLEAR_VALUE, this->SavedClearColor);
      glGetIntegerv(GL_DRAW_BUFFER, &this->SavedDrawBuffer);
      }
  }

  void RestoreGLState(bool colorBuffer = false)
  {
    this->SetGLCapability(GL_LIGHTING, this->SavedLighting);
    this->SetGLCapability(GL_DEPTH_TEST, this->SavedDepthTest);

    if (colorBuffer)
      {
      this->SetGLCapability(GL_ALPHA_TEST, this->SavedAlphaTest);
      this->SetGLCapability(GL_STENCIL_TEST, this->SavedStencilTest);
      this->SetGLCapability(GL_BLEND, this->SavedBlend);

      if(this->SavedDrawBuffer != GL_BACK_LEFT)
        {
        glDrawBuffer(this->SavedDrawBuffer);
        }

      int i = 0;
      bool colorDiffer = false;
      while(!colorDiffer && i < 4)
        {
        colorDiffer=this->SavedClearColor[i++] != 0.0;
        }
      if(colorDiffer)
        {
        glClearColor(this->SavedClearColor[0],
                     this->SavedClearColor[1],
                     this->SavedClearColor[2],
                     this->SavedClearColor[3]);
        }
      }
  }

  void SetGLCapability(GLenum capability, GLboolean state)
  {
    if (state)
      {
      glEnable(capability);
      }
    else
      {
      glDisable(capability);
      }
  }

  float* TexCoords(float* f, int n)
  {
    float* texCoord = new float[2*n];
    float minX = f[0]; float minY = f[1];
    float maxX = f[0]; float maxY = f[1];
    float* fptr = f;
    for(int i = 0; i < n; ++i)
      {
      minX = fptr[0] < minX ? fptr[0] : minX;
      maxX = fptr[0] > maxX ? fptr[0] : maxX;
      minY = fptr[1] < minY ? fptr[1] : minY;
      maxY = fptr[1] > maxY ? fptr[1] : maxY;
      fptr+=2;
      }
    fptr = f;
    if (this->TextureProperties & vtkContextDevice2D::Repeat)
      {
      double* textureBounds = this->Texture->GetInput()->GetBounds();
      float rangeX = (textureBounds[1] - textureBounds[0]) ?
        textureBounds[1] - textureBounds[0] : 1.;
      float rangeY = (textureBounds[3] - textureBounds[2]) ?
        textureBounds[3] - textureBounds[2] : 1.;
      for (int i = 0; i < n; ++i)
        {
        texCoord[i*2] = (fptr[0]-minX) / rangeX;
        texCoord[i*2+1] = (fptr[1]-minY) / rangeY;
        fptr+=2;
        }
      }
    else // this->TextureProperties & vtkContextDevice2D::Stretch
      {
      float rangeX = (maxX - minX)? maxX - minX : 1.f;
      float rangeY = (maxY - minY)? maxY - minY : 1.f;
      for (int i = 0; i < n; ++i)
        {
        texCoord[i*2] = (fptr[0]-minX)/rangeX;
        texCoord[i*2+1] = (fptr[1]-minY)/rangeY;
        fptr+=2;
        }
      }
    return texCoord;
  }

  GLuint TextureFromImage(vtkImageData *image)
  {
    if (image->GetScalarType() != VTK_UNSIGNED_CHAR)
      {
      cout << "Error = not an unsigned char..." << endl;
      return 0;
      }
    int bytesPerPixel = image->GetNumberOfScalarComponents();
    int size[3];
    image->GetDimensions(size);

    unsigned char *dataPtr =
        static_cast<unsigned char*>(image->GetScalarPointer());
    GLuint tmpIndex(0);
    GLint glFormat = bytesPerPixel == 3 ? GL_RGB : GL_RGBA;
    GLint glInternalFormat = bytesPerPixel == 3 ? GL_RGB8 : GL_RGBA8;

    glGenTextures(1, &tmpIndex);
    glBindTexture(GL_TEXTURE_2D, tmpIndex);

    glTexEnvf(GL_TEXTURE_ENV, vtkgl::COMBINE_RGB, GL_REPLACE);
    glTexEnvf(GL_TEXTURE_ENV, vtkgl::COMBINE_ALPHA, GL_REPLACE);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                     vtkgl::CLAMP_TO_EDGE );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                     vtkgl::CLAMP_TO_EDGE );

    glTexImage2D(GL_TEXTURE_2D, 0 , glInternalFormat,
                 size[0], size[1], 0, glFormat,
                 GL_UNSIGNED_BYTE, static_cast<const GLvoid *>(dataPtr));
    glAlphaFunc(GL_GREATER, static_cast<GLclampf>(0));
    glEnable(GL_ALPHA_TEST);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_TEXTURE_2D);
    return tmpIndex;
  }

  vtkTexture *Texture;
  unsigned int TextureProperties;
  vtkTexture *SpriteTexture;
  // Store the previous GL state so that we can restore it when complete
  GLboolean SavedLighting;
  GLboolean SavedDepthTest;
  GLboolean SavedAlphaTest;
  GLboolean SavedStencilTest;
  GLboolean SavedBlend;
  GLint SavedDrawBuffer;
  GLfloat SavedClearColor[4];

  int TextCounter;
  vtkVector2i Dim;
  vtkVector2i Offset;
  bool GLExtensionsLoaded;
  bool OpenGL15;
  bool OpenGL20;
  bool GLSL;
};

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkOpenGLContextDevice2D);

//-----------------------------------------------------------------------------
vtkOpenGLContextDevice2D::vtkOpenGLContextDevice2D()
{
  this->Renderer = 0;
  this->IsTextDrawn = false;
  this->InRender = false;
#ifdef VTK_USE_QT
  // Can only use the QtLabelRenderStrategy if there is a QApplication
  // instance, otherwise fallback to the FreeTypeLabelRenderStrategy.
/*  if(QApplication::instance())
    {
    this->TextRenderer = vtkQtStringToImage::New();
    }
  else
    { */
  this->TextRenderer = vtkFreeTypeStringToImage::New();
#else
  this->TextRenderer = vtkFreeTypeStringToImage::New();
#endif
  this->Storage = new vtkOpenGLContextDevice2D::Private;
  this->RenderWindow = NULL;
}

//-----------------------------------------------------------------------------
vtkOpenGLContextDevice2D::~vtkOpenGLContextDevice2D()
{
  this->TextRenderer->Delete();
  this->TextRenderer = 0;
  delete this->Storage;
  this->Storage = 0;
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::Begin(vtkViewport* viewport)
{
  // Need the actual pixel size of the viewport - ask OpenGL.
  GLint vp[4];
  glGetIntegerv(GL_VIEWPORT, vp);
  this->Storage->Offset.Set(static_cast<int>(vp[0]),
                            static_cast<int>(vp[1]));

  this->Storage->Dim.Set(static_cast<int>(vp[2]),
                         static_cast<int>(vp[3]));

  // push a 2D matrix on the stack
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  float offset = 0.5;
  glOrtho(offset, vp[2]+offset-1.0,
          offset, vp[3]+offset-1.0,
          -1, 1);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  // Store the previous state before changing it
  this->Storage->SaveGLState();
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);

  this->Renderer = vtkRenderer::SafeDownCast(viewport);
  this->IsTextDrawn = false;

  vtkOpenGLRenderer *gl = vtkOpenGLRenderer::SafeDownCast(viewport);
  if (gl)
    {
    this->RenderWindow = vtkOpenGLRenderWindow::SafeDownCast(
        gl->GetRenderWindow());
    }

  if (!this->Storage->GLExtensionsLoaded)
    {
    if (this->RenderWindow)
      {
      this->LoadExtensions(this->RenderWindow->GetExtensionManager());
      }
    }

  this->InRender = true;

  this->Modified();
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::End()
{
  if (!this->InRender)
    {
    return;
    }

  // push a 2D matrix on the stack
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  // Restore the GL state that we changed
  this->Storage->RestoreGLState();

  this->RenderWindow = NULL;

  this->InRender = false;

  this->Modified();
}

// ----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::BufferIdModeBegin(
  vtkAbstractContextBufferId *bufferId)
{
  assert("pre: not_yet" && !this->GetBufferIdMode());
  assert("pre: bufferId_exists" && bufferId!=0);

  this->BufferId=bufferId;

  // Save OpenGL state.
  this->Storage->SaveGLState(true);

  int lowerLeft[2];
  int usize, vsize;
  this->Renderer->GetTiledSizeAndOrigin(&usize,&vsize,lowerLeft,lowerLeft+1);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho( 0.5, usize+0.5,
           0.5, vsize+0.5,
          -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glDrawBuffer(GL_BACK_LEFT);
  glClearColor(0.0,0.0,0.0,0.0); // id=0 means no hit, just background
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_LIGHTING);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  assert("post: started" && this->GetBufferIdMode());
}

// ----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::BufferIdModeEnd()
{
  assert("pre: started" && this->GetBufferIdMode());

  // Assume the renderer has been set previously during rendering (sse Begin())
  int lowerLeft[2];
  int usize, vsize;
  this->Renderer->GetTiledSizeAndOrigin(&usize,&vsize,lowerLeft,lowerLeft+1);
  this->BufferId->SetValues(lowerLeft[0],lowerLeft[1]);

  // Restore OpenGL state (only if it's different to avoid too much state
  // change).
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  this->Storage->RestoreGLState(true);

  this->BufferId=0;
  assert("post: done" && !this->GetBufferIdMode());
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawPoly(float *f, int n)
{
  if(f && n > 0)
    {
    this->SetLineType(this->Pen->GetLineType());
    glColor4ubv(this->Pen->GetColor());
    glLineWidth(this->Pen->GetWidth());
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, f);
    glDrawArrays(GL_LINE_STRIP, 0, n);
    glDisableClientState(GL_VERTEX_ARRAY);
    }
  else
    {
    vtkWarningMacro(<< "Points supplied that were not of type float.");
    }
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawPoly(float *f, int n, unsigned char *c, int nc)
{
  if (f && n > 0 && nc == 4)
    {
    this->SetLineType(this->Pen->GetLineType());
    glColor4ubv(c);
    glLineWidth(this->Pen->GetWidth());
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, f);
    glDrawArrays(GL_LINE_STRIP, 0, n);
    glDisableClientState(GL_VERTEX_ARRAY);
    }
  else
    {
    vtkWarningMacro(<< "Points supplied that were not of type float.");
    }
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawPoints(float *f, int n)
{
  if (f && n > 0)
    {
    glColor4ubv(this->Pen->GetColor());
    glPointSize(this->Pen->GetWidth());
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, f);
    glDrawArrays(GL_POINTS, 0, n);
    glDisableClientState(GL_VERTEX_ARRAY);
    }
  else
    {
    vtkWarningMacro(<< "Points supplied that were not of type float.");
    }
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawPointSprites(vtkImageData *sprite,
                                                float *points, int n)
{
  if (points && n > 0)
    {
    glColor4ubv(this->Pen->GetColor());
    glPointSize(this->Pen->GetWidth());
    if (sprite)
      {
      if (!this->Storage->SpriteTexture)
        {
        this->Storage->SpriteTexture = vtkTexture::New();
        this->Storage->SpriteTexture->SetRepeat(false);
        }
      this->Storage->SpriteTexture->SetInput(sprite);
      this->Storage->SpriteTexture->Render(this->Renderer);
      }
    if (this->Storage->OpenGL15)
      {
      // We can actually use point sprites here
      glEnable(vtkgl::POINT_SPRITE);
      glTexEnvi(vtkgl::POINT_SPRITE, vtkgl::COORD_REPLACE, GL_TRUE);
      vtkgl::PointParameteri(vtkgl::POINT_SPRITE_COORD_ORIGIN, vtkgl::LOWER_LEFT);

      this->DrawPoints(points, n);

      glTexEnvi(vtkgl::POINT_SPRITE, vtkgl::COORD_REPLACE, GL_FALSE);
      glDisable(vtkgl::POINT_SPRITE);

      }
    else
      {
      // Must emulate the point sprites - slower but at least they see something.
      GLfloat width = 1.0;
      glGetFloatv(GL_POINT_SIZE, &width);
      width /= 2.0;

      // Need to get the model view matrix for scaling factors...
      GLfloat mv[16];
      glGetFloatv(GL_MODELVIEW_MATRIX, mv);
      float xWidth = width / mv[0];
      float yWidth = width / mv[5];

      float p[8] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

      // This will be the same everytime
      float texCoord[] = { 0.0, 0.0,
                           1.0, 0.0,
                           1.0, 1.0,
                           0.0, 1.0 };

      glEnableClientState(GL_VERTEX_ARRAY);
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
      glTexCoordPointer(2, GL_FLOAT, 0, texCoord);

      for (int i = 0; i < n; ++i)
        {
        p[0] = points[2*i] - xWidth;
        p[1] = points[2*i+1] - yWidth;
        p[2] = points[2*i] + xWidth;
        p[3] = points[2*i+1] - yWidth;
        p[4] = points[2*i] + xWidth;
        p[5] = points[2*i+1] + yWidth;
        p[6] = points[2*i] - xWidth;
        p[7] = points[2*i+1] + yWidth;

        glVertexPointer(2, GL_FLOAT, 0, p);
        glDrawArrays(GL_QUADS, 0, 4);
        }
      glDisableClientState(GL_TEXTURE_COORD_ARRAY);
      glDisableClientState(GL_VERTEX_ARRAY);
      }
    if (sprite)
      {
      this->Storage->SpriteTexture->PostRender(this->Renderer);
      glDisable(GL_TEXTURE_2D);
      }
    }
  else
    {
    vtkWarningMacro(<< "Points supplied without a valid image or pointer.");
    }
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawPoints(float *f, int n, unsigned char *c, int nc)
{
  if (f && n > 0)
    {
    glPointSize(this->Pen->GetWidth());
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(nc, GL_UNSIGNED_BYTE, 0, c);
    glVertexPointer(2, GL_FLOAT, 0, f);
    glDrawArrays(GL_POINTS, 0, n);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    }
  else
    {
    vtkWarningMacro(<< "Points supplied that were not of type float.");
    }
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawPointSprites(vtkImageData *sprite,
                    float *points, int n, unsigned char *colors, int nc_comps)
{
  if (points && n > 0)
    {
    // glColor4ubv(this->Pen->GetColor());
    glPointSize(this->Pen->GetWidth());
    if (sprite)
      {
      if (!this->Storage->SpriteTexture)
        {
        this->Storage->SpriteTexture = vtkTexture::New();
        this->Storage->SpriteTexture->SetRepeat(false);
        }
      this->Storage->SpriteTexture->SetInput(sprite);
      this->Storage->SpriteTexture->Render(this->Renderer);
      }
    if (this->Storage->OpenGL15)
      {
      // We can actually use point sprites here
      glEnable(vtkgl::POINT_SPRITE);
      glTexEnvi(vtkgl::POINT_SPRITE, vtkgl::COORD_REPLACE, GL_TRUE);
      vtkgl::PointParameteri(vtkgl::POINT_SPRITE_COORD_ORIGIN, vtkgl::LOWER_LEFT);

      this->DrawPoints(points, n, colors, nc_comps);

      glTexEnvi(vtkgl::POINT_SPRITE, vtkgl::COORD_REPLACE, GL_FALSE);
      glDisable(vtkgl::POINT_SPRITE);

      }
    else
      {
      // Must emulate the point sprites - slower but at least they see something.
      GLfloat width = 1.0;
      glGetFloatv(GL_POINT_SIZE, &width);
      width /= 2.0;

      // Need to get the model view matrix for scaling factors...
      GLfloat mv[16];
      glGetFloatv(GL_MODELVIEW_MATRIX, mv);
      float xWidth = width / mv[0];
      float yWidth = width / mv[5];

      float p[8] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
      float c[8] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

      // This will be the same everytime
      float texCoord[] = { 0.0, 0.0,
                           1.0, 0.0,
                           1.0, 1.0,
                           0.0, 1.0 };

      glEnableClientState(GL_VERTEX_ARRAY);
      glEnableClientState(GL_COLOR_ARRAY);
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
      glTexCoordPointer(2, GL_FLOAT, 0, texCoord);

      for (int i = 0; i < n; ++i)
        {
        p[0] = points[2*i] - xWidth;
        p[1] = points[2*i+1] - yWidth;
        p[2] = points[2*i] + xWidth;
        p[3] = points[2*i+1] - yWidth;
        p[4] = points[2*i] + xWidth;
        p[5] = points[2*i+1] + yWidth;
        p[6] = points[2*i] - xWidth;
        p[7] = points[2*i+1] + yWidth;

        for (int j = 0; j < 8; ++j)
          {
          c[j] = colors[nc_comps*i];
          }

        glVertexPointer(2, GL_FLOAT, 0, p);
        glColorPointer(nc_comps, GL_UNSIGNED_BYTE, 0, c);
        glDrawArrays(GL_QUADS, 0, 4);
        }
      glDisableClientState(GL_TEXTURE_COORD_ARRAY);
      glDisableClientState(GL_COLOR_ARRAY);
      glDisableClientState(GL_VERTEX_ARRAY);
      }
    if (sprite)
      {
      this->Storage->SpriteTexture->PostRender(this->Renderer);
      glDisable(GL_TEXTURE_2D);
      }
    }
  else
    {
    vtkWarningMacro(<< "Points supplied without a valid image or pointer.");
    }
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawQuad(float *f, int n)
{
  if (!f || n <= 0)
    {
    vtkWarningMacro(<< "Points supplied that were not of type float.");
    return;
    }
  glColor4ubv(this->Brush->GetColor());
  float* texCoord = 0;
  if (this->Brush->GetTexture())
    {
    this->SetTexture(this->Brush->GetTexture(),
                     this->Brush->GetTextureProperties());
    this->Storage->Texture->Render(this->Renderer);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    texCoord = this->Storage->TexCoords(f, n);
    glTexCoordPointer(2, GL_FLOAT, 0, &texCoord[0]);
    }
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 0, f);
  glDrawArrays(GL_QUADS, 0, n);
  glDisableClientState(GL_VERTEX_ARRAY);
  if (this->Storage->Texture)
    {
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    this->Storage->Texture->PostRender(this->Renderer);
    glDisable(GL_TEXTURE_2D);
    delete [] texCoord;
    }
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawQuadStrip(float *f, int n)
{
  if (!f || n <= 0)
    {
    vtkWarningMacro(<< "Points supplied that were not of type float.");
    return;
    }
  glColor4ubv(this->Brush->GetColor());
  float* texCoord = 0;
  if (this->Brush->GetTexture())
    {
    this->SetTexture(this->Brush->GetTexture(),
                     this->Brush->GetTextureProperties());
    this->Storage->Texture->Render(this->Renderer);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    texCoord = this->Storage->TexCoords(f, n);
    glTexCoordPointer(2, GL_FLOAT, 0, &texCoord[0]);
    }
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 0, f);
  glDrawArrays(GL_QUAD_STRIP, 0, n);
  glDisableClientState(GL_VERTEX_ARRAY);
  if (this->Storage->Texture)
    {
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    this->Storage->Texture->PostRender(this->Renderer);
    glDisable(GL_TEXTURE_2D);
    delete [] texCoord;
    }
}
//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawPolygon(float *f, int n)
{
  if (!f || n <= 0)
    {
    vtkWarningMacro(<< "Points supplied that were not of type float.");
    return;
    }
  glColor4ubv(this->Brush->GetColor());
  float* texCoord = 0;
  if (this->Brush->GetTexture())
    {
    this->SetTexture(this->Brush->GetTexture(),
                     this->Brush->GetTextureProperties());
    this->Storage->Texture->Render(this->Renderer);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    texCoord = this->Storage->TexCoords(f, n);
    glTexCoordPointer(2, GL_FLOAT, 0, &texCoord[0]);
    }
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 0, f);
  glDrawArrays(GL_POLYGON, 0, n);
  glDisableClientState(GL_VERTEX_ARRAY);
  if (this->Storage->Texture)
    {
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    this->Storage->Texture->PostRender(this->Renderer);
    glDisable(GL_TEXTURE_2D);
    delete [] texCoord;
    }
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawEllipseWedge(float x, float y, float outRx,
                                                float outRy, float inRx,
                                                float inRy, float startAngle,
                                                float stopAngle)

{
  assert("pre: positive_outRx" && outRx>=0.0f);
  assert("pre: positive_outRy" && outRy>=0.0f);
  assert("pre: positive_inRx" && inRx>=0.0f);
  assert("pre: positive_inRy" && inRy>=0.0f);
  assert("pre: ordered_rx" && inRx<=outRx);
  assert("pre: ordered_ry" && inRy<=outRy);

  if(outRy==0.0f && outRx==0.0f)
    {
    // we make sure maxRadius will never be null.
    return;
    }

  int iterations=this->GetNumberOfArcIterations(outRx,outRy,startAngle,
                                                stopAngle);

  float *p=new float[4*(iterations+1)];

  // step in radians.
  double step =
    vtkMath::RadiansFromDegrees(stopAngle-startAngle)/(iterations);

  // step have to be lesser or equal to maxStep computed inside
  // GetNumberOfIterations()

  double rstart=vtkMath::RadiansFromDegrees(startAngle);

  // the A vertices (0,2,4,..) are on the inner side
  // the B vertices (1,3,5,..) are on the outer side
  // (A and B vertices terms come from triangle strip definition in
  // OpenGL spec)
  // we are iterating counterclockwise

  int i=0;
  while(i<=iterations)
    {
    // A vertex (inner side)
    double a=rstart+i*step;
    p[4*i  ] = inRx * cos(a) + x;
    p[4*i+1] = inRy * sin(a) + y;

    // B vertex (outer side)
    p[4*i+2] = outRx * cos(a) + x;
    p[4*i+3] = outRy * sin(a) + y;

    ++i;
    }

  glColor4ubv(this->Brush->GetColor());
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 0, p);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 2*(iterations+1));
  glDisableClientState(GL_VERTEX_ARRAY);

  delete[] p;
}

// ----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawEllipticArc(float x, float y, float rX,
                                               float rY, float startAngle,
                                               float stopAngle)
{
  assert("pre: positive_rX" && rX>=0);
  assert("pre: positive_rY" && rY>=0);

  if(rX==0.0f && rY==0.0f)
    {
    // we make sure maxRadius will never be null.
    return;
    }
  int iterations = this->GetNumberOfArcIterations(rX, rY, startAngle, stopAngle);

  float *p = new float[2*(iterations+1)];

  // step in radians.
  double step =
    vtkMath::RadiansFromDegrees(stopAngle-startAngle)/(iterations);

  // step have to be lesser or equal to maxStep computed inside
  // GetNumberOfIterations()
  double rstart=vtkMath::RadiansFromDegrees(startAngle);

  // we are iterating counterclockwise
  for(int i = 0; i <= iterations; ++i)
    {
    double a=rstart+i*step;
    p[2*i  ] = rX * cos(a) + x;
    p[2*i+1] = rY * sin(a) + y;
    }

  this->SetLineType(this->Pen->GetLineType());
  glColor4ubv(this->Pen->GetColor());
  glLineWidth(this->Pen->GetWidth());
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 0, p);
  glDrawArrays(GL_LINE_STRIP, 0, iterations+1);
  glColor4ubv(this->Brush->GetColor());
  glDrawArrays(GL_TRIANGLE_FAN, 0, iterations+1);
  glDisableClientState(GL_VERTEX_ARRAY);

  delete[] p;
}

// ----------------------------------------------------------------------------
int vtkOpenGLContextDevice2D::GetNumberOfArcIterations(float rX,
                                                       float rY,
                                                       float startAngle,
                                                       float stopAngle)
{
  assert("pre: positive_rX" && rX>=0.0f);
  assert("pre: positive_rY" && rY>=0.0f);
  assert("pre: not_both_null" && (rX>0.0 || rY>0.0));

// 1.0: pixel precision. 0.5 (subpixel precision, useful with multisampling)
  double error = 4.0; // experience shows 4.0 is visually enough.

  // The tessellation is the most visible on the biggest radius.
  double maxRadius;
  if(rX >= rY)
    {
    maxRadius = rX;
    }
  else
    {
    maxRadius = rY;
    }

  if(error > maxRadius)
    {
    // to make sure the argument of asin() is in a valid range.
    error = maxRadius;
    }

  // Angle of a sector so that its chord is `error' pixels.
  // This is will be our maximum angle step.
  double maxStep = 2.0 * asin(error / (2.0 * maxRadius));

  // ceil because we want to make sure we don't underestimate the number of
  // iterations by 1.
  return static_cast<int>(
    ceil(vtkMath::RadiansFromDegrees(stopAngle - startAngle) / maxStep));
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::AlignText(double orientation, int *extent,
                                         float *p)
{
  // Special case multiples of 90 as no transformation is required...
  if (orientation > -0.0001 && orientation < 0.0001)
    {
    switch (this->TextProp->GetJustification())
      {
      case VTK_TEXT_LEFT:
        break;
      case VTK_TEXT_CENTERED:
        p[0] -= floor(extent[1] / 2.0);
        break;
      case VTK_TEXT_RIGHT:
        p[0] -= extent[1];
        break;
      }
    switch (this->TextProp->GetVerticalJustification())
      {
      case VTK_TEXT_BOTTOM:
        break;
      case VTK_TEXT_CENTERED:
        p[1] -= floor(extent[3] / 2.0);
        break;
      case VTK_TEXT_TOP:
        p[1] -= extent[3];
        break;
      }
    }
  else if (orientation > 89.9999 && orientation < 90.0001)
    {
    switch (this->TextProp->GetJustification())
      {
      case VTK_TEXT_LEFT:
        break;
      case VTK_TEXT_CENTERED:
        p[1] -= floor(extent[3] / 2.0);
        break;
      case VTK_TEXT_RIGHT:
        p[1] -= extent[3];
        break;
      }
    switch (this->TextProp->GetVerticalJustification())
      {
      case VTK_TEXT_TOP:
        break;
      case VTK_TEXT_CENTERED:
        p[0] -= floor(extent[1] / 2.0);
        break;
      case VTK_TEXT_BOTTOM:
        p[0] -= extent[1];
        break;
      }
    }
  else if (orientation > 179.9999 && orientation < 180.0001)
    {
    switch (this->TextProp->GetJustification())
      {
      case VTK_TEXT_RIGHT:
        break;
      case VTK_TEXT_CENTERED:
        p[0] -= floor(extent[1] / 2.0);
        break;
      case VTK_TEXT_LEFT:
        p[0] -= extent[1];
        break;
      }
    switch (this->TextProp->GetVerticalJustification())
      {
      case VTK_TEXT_TOP:
        break;
      case VTK_TEXT_CENTERED:
        p[1] -= floor(extent[3] / 2.0);
        break;
      case VTK_TEXT_BOTTOM:
        p[1] -= extent[3];
        break;
      }
    }
  else if (orientation > 269.9999 && orientation < 270.0001)
    {
    switch (this->TextProp->GetJustification())
      {
      case VTK_TEXT_LEFT:
        break;
      case VTK_TEXT_CENTERED:
        p[1] -= floor(extent[3] / 2.0);
        break;
      case VTK_TEXT_RIGHT:
        p[1] -= extent[3];
        break;
      }
    switch (this->TextProp->GetVerticalJustification())
      {
      case VTK_TEXT_BOTTOM:
        break;
      case VTK_TEXT_CENTERED:
        p[0] -= floor(extent[1] / 2.0);
        break;
      case VTK_TEXT_TOP:
        p[0] -= extent[1];
        break;
      }
    }
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawString(float *point,
                                          const vtkStdString &string)
{
  float p[] = { floor(point[0]), floor(point[1]) };

  vtkImageData *image = vtkImageData::New();
  if (!this->TextRenderer->RenderString(this->TextProp, string, image))
    {
    image->Delete();
    return;
    }

  this->SetTexture(image);
  this->Storage->Texture->Render(this->Renderer);
  int *extent = image->GetExtent();

  p[0] -= extent[0] - 0.5;
  p[1] -= extent[2] - 0.5;

  this->AlignText(this->TextProp->GetOrientation(), extent, p);

  float points[] = { p[0]              , p[1],
                     p[0]+extent[1]+1.0, p[1],
                     p[0]+extent[1]+1.0, p[1]+extent[3]+1.0,
                     p[0]              , p[1]+extent[3]+1.0 };

  float texCoord[] = { 0.0, 0.0,
                       1.0, 0.0,
                       1.0, 1.0,
                       0.0, 1.0 };

  glColor4ub(255, 255, 255, 255);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glVertexPointer(2, GL_FLOAT, 0, points);
  glTexCoordPointer(2, GL_FLOAT, 0, texCoord);
  glDrawArrays(GL_QUADS, 0, 4);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);

  this->Storage->Texture->PostRender(this->Renderer);
  glDisable(GL_TEXTURE_2D);
  image->Delete();
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::ComputeStringBounds(const vtkStdString &string,
                                                   float bounds[4])
{
  vtkVector2i box = this->TextRenderer->GetBounds(this->TextProp, string);
  bounds[0] = static_cast<float>(0);
  bounds[1] = static_cast<float>(0);
  bounds[2] = static_cast<float>(box.X());
  bounds[3] = static_cast<float>(box.Y());
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawString(float *point,
                                          const vtkUnicodeString &string)
{
  int p[] = { static_cast<int>(point[0]),
              static_cast<int>(point[1]) };

  //TextRenderer draws in window, not viewport coords
  p[0]+=this->Storage->Offset.GetX();
  p[1]+=this->Storage->Offset.GetY();
  vtkImageData *data = vtkImageData::New();
  this->TextRenderer->RenderString(this->TextProp, string, data);
  this->DrawImage(point, 1.0, data);
  data->Delete();
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::ComputeStringBounds(const vtkUnicodeString &string,
                                                   float bounds[4])
{
  vtkVector2i box = this->TextRenderer->GetBounds(this->TextProp, string);
  bounds[0] = static_cast<float>(0);
  bounds[1] = static_cast<float>(0);
  bounds[2] = static_cast<float>(box.X());
  bounds[3] = static_cast<float>(box.Y());
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawImage(float p[2], float scale,
                                         vtkImageData *image)
{
  this->SetTexture(image);
  this->Storage->Texture->Render(this->Renderer);
  int *extent = image->GetExtent();
  float points[] = { p[0]                    , p[1],
                     p[0]+scale*extent[1]+1.0, p[1],
                     p[0]+scale*extent[1]+1.0, p[1]+scale*extent[3]+1.0,
                     p[0]                    , p[1]+scale*extent[3]+1.0 };

  float texCoord[] = { 0.0, 0.0,
                       1.0, 0.0,
                       1.0, 1.0,
                       0.0, 1.0 };

  glColor4ub(255, 255, 255, 255);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glVertexPointer(2, GL_FLOAT, 0, &points[0]);
  glTexCoordPointer(2, GL_FLOAT, 0, &texCoord[0]);
  glDrawArrays(GL_QUADS, 0, 4);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);

  this->Storage->Texture->PostRender(this->Renderer);
  glDisable(GL_TEXTURE_2D);
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DrawImage(const vtkRectf& pos,
                                         vtkImageData *image)
{
  GLuint index = this->Storage->TextureFromImage(image);
//  this->SetTexture(image);
//  this->Storage->Texture->Render(this->Renderer);
  float points[] = { pos.X()              , pos.Y(),
                     pos.X() + pos.Width(), pos.Y(),
                     pos.X() + pos.Width(), pos.Y() + pos.Height(),
                     pos.X()              , pos.Y() + pos.Height() };

  float texCoord[] = { 0.0, 0.0,
                       1.0, 0.0,
                       1.0, 1.0,
                       0.0, 1.0 };

  glColor4ub(255, 255, 255, 255);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glVertexPointer(2, GL_FLOAT, 0, &points[0]);
  glTexCoordPointer(2, GL_FLOAT, 0, &texCoord[0]);
  glDrawArrays(GL_QUADS, 0, 4);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);

//  this->Storage->Texture->PostRender(this->Renderer);
  glDisable(GL_TEXTURE_2D);
  glDeleteTextures(1, &index);
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::SetColor4(unsigned char *color)
{
  glColor4ubv(color);
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::SetColor(unsigned char *color)
{
  glColor3ubv(color);
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::SetTexture(vtkImageData* image, int properties)
{
  if (image == NULL)
    {
    if (this->Storage->Texture)
      {
      this->Storage->Texture->Delete();
      this->Storage->Texture = 0;
      }
    return;
    }
  if (this->Storage->Texture == NULL)
    {
    this->Storage->Texture = vtkTexture::New();
    }
  this->Storage->Texture->SetInput(image);
  this->Storage->TextureProperties = properties;
  this->Storage->Texture->SetRepeat(properties & vtkContextDevice2D::Repeat);
  this->Storage->Texture->SetInterpolate(properties & vtkContextDevice2D::Linear);
  this->Storage->Texture->EdgeClampOn();
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::SetPointSize(float size)
{
  glPointSize(size);
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::SetLineWidth(float width)
{
  glLineWidth(width);
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::SetLineType(int type)
{
  if (type == vtkPen::SOLID_LINE)
    {
    glDisable(GL_LINE_STIPPLE);
    }
  else
    {
    glEnable(GL_LINE_STIPPLE);
    }
  GLushort pattern = 0x0000;
  switch (type)
    {
    case vtkPen::NO_PEN:
      pattern = 0x0000;
      break;
    case vtkPen::DASH_LINE:
      pattern = 0x00FF;
      break;
    case vtkPen::DOT_LINE:
      pattern = 0x0101;
      break;
    case vtkPen::DASH_DOT_LINE:
      pattern = 0x0C0F;
      break;
    case vtkPen::DASH_DOT_DOT_LINE:
      pattern = 0x1C47;
      break;
    default:
      pattern = 0x0000;
    }
  glLineStipple(1, pattern);
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::MultiplyMatrix(vtkMatrix3x3 *m)
{
  // We must construct a 4x4 matrix from the 3x3 matrix for OpenGL
  double *M = m->GetData();
  double matrix[16];

  // Convert from row major (C++ two dimensional arrays) to OpenGL
  matrix[0] = M[0];
  matrix[1] = M[3];
  matrix[2] = 0.0;
  matrix[3] = M[6];

  matrix[4] = M[1];
  matrix[5] = M[4];
  matrix[6] = 0.0;
  matrix[7] = M[7];

  matrix[8] = 0.0;
  matrix[9] = 0.0;
  matrix[10] = 1.0;
  matrix[11] = 0.0;

  matrix[12] = M[2];
  matrix[13] = M[5];
  matrix[14] = 0.0;
  matrix[15] = M[8];

  glMultMatrixd(matrix);
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::SetMatrix(vtkMatrix3x3 *m)
{
  // We must construct a 4x4 matrix from the 3x3 matrix for OpenGL
  double *M = m->GetData();
  double matrix[16];

  // Convert from row major (C++ two dimensional arrays) to OpenGL
  matrix[0] = M[0];
  matrix[1] = M[3];
  matrix[2] = 0.0;
  matrix[3] = M[6];

  matrix[4] = M[1];
  matrix[5] = M[4];
  matrix[6] = 0.0;
  matrix[7] = M[7];

  matrix[8] = 0.0;
  matrix[9] = 0.0;
  matrix[10] = 1.0;
  matrix[11] = 0.0;

  matrix[12] = M[2];
  matrix[13] = M[5];
  matrix[14] = 0.0;
  matrix[15] = M[8];

  glLoadMatrixd(matrix);
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::GetMatrix(vtkMatrix3x3 *m)
{
  assert("pre: non_null" && m != NULL);
  // We must construct a 4x4 matrix from the 3x3 matrix for OpenGL
  double *M = m->GetData();
  double matrix[16];
  glGetDoublev(GL_MODELVIEW_MATRIX, matrix);

  // Convert from row major (C++ two dimensional arrays) to OpenGL
  M[0] = matrix[0];
  M[1] = matrix[4];
  M[2] = matrix[12];
  M[3] = matrix[1];
  M[4] = matrix[5];
  M[5] = matrix[13];
  M[6] = matrix[3];
  M[7] = matrix[7];
  M[8] = matrix[15];

  m->Modified();
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::PushMatrix()
{
  glMatrixMode( GL_MODELVIEW );
  glPushMatrix();
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::PopMatrix()
{
  glMatrixMode( GL_MODELVIEW );
  glPopMatrix();
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::SetClipping(int *dim)
{
  // Check the bounds, and clamp if necessary
  GLint vp[4] = { this->Storage->Offset.GetX(), this->Storage->Offset.GetY(),
    this->Storage->Dim.GetX(),this->Storage->Dim.GetY()};

  if (dim[0] > 0 && dim[0] < vp[2] )
    {
    vp[0] += dim[0];
    }
  if (dim[1] > 0 && dim[1] < vp[3])
    {
    vp[1] += dim[1];
    }
  if (dim[2] > 0 && dim[2] < vp[2])
    {
    vp[2] = dim[2];
    }
  if (dim[3] > 0 && dim[3] < vp[3])
    {
    vp[3] = dim[3];
    }

  glScissor(vp[0], vp[1], vp[2], vp[3]);
  glEnable(GL_SCISSOR_TEST);
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::DisableClipping()
{
  glDisable(GL_SCISSOR_TEST);
}

//-----------------------------------------------------------------------------
bool vtkOpenGLContextDevice2D::SetStringRendererToFreeType()
{
#ifdef VTK_USE_QT
  // We will likely be using the Qt rendering strategy
  if (this->TextRenderer->IsA("vtkQtStringToImage"))
    {
    this->TextRenderer->Delete();
    this->TextRenderer = vtkFreeTypeStringToImage::New();
    }
#endif
  // FreeType is the only choice - nothing to do here
  return true;
}

//-----------------------------------------------------------------------------
bool vtkOpenGLContextDevice2D::SetStringRendererToQt()
{
#ifdef VTK_USE_QT
  // We will likely be using the Qt rendering strategy
  if (this->TextRenderer->IsA("vtkQtStringToImage"))
    {
    return true;
    }
  else
    {
    this->TextRenderer->Delete();
    this->TextRenderer = vtkQtStringToImage::New();
    }
#endif
  // The Qt based strategy is not available
  return false;
}

//----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::ReleaseGraphicsResources(vtkWindow *window)
{
  if (this->Storage->Texture)
    {
    this->Storage->Texture->ReleaseGraphicsResources(window);
    }
  if (this->Storage->SpriteTexture)
    {
    this->Storage->SpriteTexture->ReleaseGraphicsResources(window);
    }
}

//----------------------------------------------------------------------------
bool vtkOpenGLContextDevice2D::HasGLSL()
{
  return this->Storage->GLSL;
}

//-----------------------------------------------------------------------------
bool vtkOpenGLContextDevice2D::LoadExtensions(vtkOpenGLExtensionManager *m)
{
  if(m->ExtensionSupported("GL_VERSION_2_0"))
    {
    m->LoadExtension("GL_VERSION_2_0");
    this->Storage->OpenGL20 = true;
    }
  else
    {
    this->Storage->OpenGL20 = false;
    }
  if(m->ExtensionSupported("GL_VERSION_1_5"))
    {
    m->LoadExtension("GL_VERSION_1_5");
    this->Storage->OpenGL15 = true;
    }
  else
    {
    this->Storage->OpenGL15 = false;
    }
  if(vtkShaderProgram2::IsSupported(
      static_cast<vtkOpenGLRenderWindow *>(m->GetRenderWindow())))
    {
    this->Storage->GLSL = true;
    }
  else
    {
    this->Storage->GLSL = false;
    }

  this->Storage->GLExtensionsLoaded = true;
  return true;
}

//-----------------------------------------------------------------------------
void vtkOpenGLContextDevice2D::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Renderer: ";
  if (this->Renderer)
  {
    os << endl;
    this->Renderer->PrintSelf(os, indent.GetNextIndent());
  }
  else
    {
    os << "(none)" << endl;
    }
  os << indent << "Text Renderer: ";
  if (this->Renderer)
  {
    os << endl;
    this->TextRenderer->PrintSelf(os, indent.GetNextIndent());
  }
  else
    {
    os << "(none)" << endl;
    }
}
