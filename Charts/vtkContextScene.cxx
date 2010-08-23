/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkContextScene.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkContextScene.h"

#include "vtkContextItem.h"
#include "vtkContext2D.h"
#include "vtkTransform2D.h"
#include "vtkMatrix3x3.h"
#include "vtkContextScenePrivate.h"
#include "vtkContextMouseEvent.h"

// Get my new commands
#include "vtkCommand.h"

#include "vtkAnnotationLink.h"
#include "vtkInteractorStyle.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderWindow.h"
#include "vtkRenderer.h"
#include "vtkInteractorStyleRubberBand2D.h"
#include "vtkObjectFactory.h"
#include "vtkContextBufferId.h"
#include "vtkOpenGLContextBufferId.h"
#include "vtkOpenGLRenderWindow.h"

// My STL containers
#include <vtkstd/vector>
#include <assert.h>

//-----------------------------------------------------------------------------
// Minimal command class to handle callbacks.
class vtkContextScene::Command : public vtkCommand
{
public:
  Command(vtkContextScene *scene) { this->Target = scene; }

  virtual void Execute(vtkObject *caller, unsigned long eventId, void *callData)
    {
    if (this->Target)
      {
      vtkInteractorStyle *style = vtkInteractorStyle::SafeDownCast(caller);
      vtkRenderWindowInteractor *interactor = NULL;
      if (style)
        {
        interactor = vtkRenderWindowInteractor::SafeDownCast(style->GetInteractor());
        }

      int x = -1;
      int y = -1;
      if (interactor)
        {
        x = interactor->GetEventPosition()[0];
        y = interactor->GetEventPosition()[1];
        }
      else
        {
        return;
        }

//      cout << "eventId: " << eventId << " -> "
//           << this->GetStringFromEventId(eventId) << endl;

      switch (eventId)
        {
        case vtkCommand::MouseMoveEvent :
          this->Target->MouseMoveEvent(x, y);
          break;
        case vtkCommand::LeftButtonPressEvent :
          this->Target->ButtonPressEvent(vtkContextMouseEvent::LEFT_BUTTON, x, y);
          break;
        case vtkCommand::MiddleButtonPressEvent :
          this->Target->ButtonPressEvent(vtkContextMouseEvent::MIDDLE_BUTTON, x, y);
          break;
        case vtkCommand::RightButtonPressEvent :
          this->Target->ButtonPressEvent(vtkContextMouseEvent::RIGHT_BUTTON, x, y);
          break;
        case vtkCommand::LeftButtonReleaseEvent :
          this->Target->ButtonReleaseEvent(vtkContextMouseEvent::LEFT_BUTTON, x, y);
          break;
        case vtkCommand::MiddleButtonReleaseEvent :
          this->Target->ButtonReleaseEvent(vtkContextMouseEvent::MIDDLE_BUTTON, x, y);
          break;
        case vtkCommand::RightButtonReleaseEvent :
          this->Target->ButtonReleaseEvent(vtkContextMouseEvent::RIGHT_BUTTON, x, y);
          break;
        case vtkCommand::MouseWheelForwardEvent :
          // There is a forward and a backward event - not clear on deltas...
          this->Target->MouseWheelEvent(+1, x, y);
          break;
        case vtkCommand::MouseWheelBackwardEvent :
          // There is a forward and a backward event - not clear on deltas...
          this->Target->MouseWheelEvent(-1, x, y);
          break;
        case vtkCommand::SelectionChangedEvent :
          this->Target->ProcessSelectionEvent(caller, callData);
          break;
        default:
          this->Target->ProcessEvents(caller, eventId, callData);
        }
      this->Target->CheckForRepaint();
      }
    }

  void SetTarget(vtkContextScene* t) { this->Target = t; }

  vtkContextScene *Target;
};

//-----------------------------------------------------------------------------
// Minimal storage class for STL containers etc.
class vtkContextScene::Private
{
public:
  Private()
    {
      this->itemMousePressCurrent = -1;
    this->Event.Button = vtkContextMouseEvent::NO_BUTTON;
    this->IsDirty = true;
    }
  ~Private()
    {
    }

  int itemMousePressCurrent; // Index of the item with a current mouse down
  vtkContextMouseEvent Event; // Mouse event structure
  bool IsDirty;
};

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkContextScene);
vtkCxxSetObjectMacro(vtkContextScene, AnnotationLink, vtkAnnotationLink);

//-----------------------------------------------------------------------------
vtkContextScene::vtkContextScene()
{
  this->Observer = new vtkContextScene::Command(this);
  this->Storage = new Private;
  this->AnnotationLink = NULL;
  this->Geometry[0] = 0;
  this->Geometry[1] = 0;
  this->BufferId=0;
  this->BufferIdDirty=true;
  this->BufferIdSupportTested=false;
  this->BufferIdSupported=false;
  this->UseBufferId = true;
  this->Transform = NULL;
  this->Children = new vtkContextScenePrivate;
  this->Children->SetScene(this);
}

//-----------------------------------------------------------------------------
vtkContextScene::~vtkContextScene()
{
  this->Observer->Delete();
  this->Observer = NULL;
  delete this->Storage;
  this->Storage = NULL;
  this->SetAnnotationLink(NULL);
  if(this->BufferId!=0)
    {
    this->BufferId->Delete();
    }
  if (this->Transform)
    {
    this->Transform->Delete();
    }
  delete this->Children;
}

//-----------------------------------------------------------------------------
void vtkContextScene::SetRenderer(vtkRenderer *r)
{
  this->Renderer=r;
  this->BufferIdSupportTested=false;
}

//-----------------------------------------------------------------------------
bool vtkContextScene::Paint(vtkContext2D *painter)
{
  vtkDebugMacro("Paint event called.");
  size_t size = this->Children->size();
  if (size && this->Transform)
    {
    painter->PushMatrix();
    painter->SetTransform(this->Transform);
    }
  this->Children->PaintItems(painter);
  if (size && this->Transform)
    {
    painter->PopMatrix();
    }
  if(this->Storage->IsDirty)
    {
    this->BufferIdDirty=true;
    }
  this->Storage->IsDirty = false;
  this->LastPainter=painter;
  return true;
}

//-----------------------------------------------------------------------------
void vtkContextScene::PaintIds()
{
  vtkDebugMacro("PaintId called.");
  size_t size = this->Children->size();

  if(size>16777214) // 24-bit limit, 0 reserved for background encoding.
    {
    vtkWarningMacro(<<"picking will not work properly as there are two many items. Items over 16777214 will be ignored.");
    size=16777214;
    }
  for (size_t i = 0; i < size; ++i)
    {
    this->LastPainter->ApplyId(i+1);
    (*this->Children)[i]->Paint(this->LastPainter);
    }
  this->Storage->IsDirty = false;
}

//-----------------------------------------------------------------------------
unsigned int vtkContextScene::AddItem(vtkAbstractContextItem *item)
{
  return this->Children->AddItem(item);
}

//-----------------------------------------------------------------------------
bool vtkContextScene::RemoveItem(vtkAbstractContextItem* item)
{
  return this->Children->RemoveItem(item);
}

//-----------------------------------------------------------------------------
bool vtkContextScene::RemoveItem(unsigned int index)
{
  return this->Children->RemoveItem(index);
}

//-----------------------------------------------------------------------------
vtkAbstractContextItem * vtkContextScene::GetItem(unsigned int index)
{
  if (index < this->Children->size())
    {
    return this->Children->at(index);
    }
  else
    {
    return 0;
    }
}

//-----------------------------------------------------------------------------
unsigned int vtkContextScene::GetNumberOfItems()
{
  return static_cast<unsigned int>(this->Children->size());
}

//-----------------------------------------------------------------------------
void vtkContextScene::ClearItems()
{
  this->Children->Clear();
}


//-----------------------------------------------------------------------------
int vtkContextScene::GetViewWidth()
{
  if (this->Renderer)
    {
    return this->Renderer->GetRenderWindow()->GetSize()[0];
    }
  else
    {
    return 0;
    }
}

//-----------------------------------------------------------------------------
int vtkContextScene::GetViewHeight()
{
  if (this->Renderer)
    {
    return this->Renderer->GetRenderWindow()->GetSize()[1];
    }
  else
    {
    return 0;
    }
}

//-----------------------------------------------------------------------------
int vtkContextScene::GetSceneWidth()
{
  return this->Geometry[0];
}

//-----------------------------------------------------------------------------
int vtkContextScene::GetSceneHeight()
{
  return this->Geometry[1];
}

//-----------------------------------------------------------------------------
void vtkContextScene::SetInteractorStyle(vtkInteractorStyle *interactor)
{
  //cout << "Interactor style " << interactor << " " << interactor->GetClassName() << endl;
  interactor->AddObserver(vtkCommand::SelectionChangedEvent, this->Observer);
  interactor->AddObserver(vtkCommand::AnyEvent, this->Observer);
}

//-----------------------------------------------------------------------------
void vtkContextScene::ProcessEvents(vtkObject* caller, unsigned long eventId,
                             void*)
{
  vtkDebugMacro("ProcessEvents called! " << caller->GetClassName() << "\t"
      << vtkCommand::GetStringFromEventId(eventId)
      << "\n\t" << vtkInteractorStyleRubberBand2D::SafeDownCast(caller)->GetInteraction());
}

//-----------------------------------------------------------------------------
void vtkContextScene::SetDirty(bool isDirty)
{
  this->Storage->IsDirty = isDirty;
  if(this->Storage->IsDirty)
    {
    this->BufferIdDirty=true;
    }
}

// ----------------------------------------------------------------------------
void vtkContextScene::ReleaseGraphicsResources()
{
  if(this->BufferId!=0)
    {
    this->BufferId->ReleaseGraphicsResources();
    }
  for(vtkContextScenePrivate::const_iterator it = this->Children->begin();
    it != this->Children->end(); ++it)
    {
    (*it)->ReleaseGraphicsResources();
    }
}

//-----------------------------------------------------------------------------
vtkWeakPointer<vtkContext2D> vtkContextScene::GetLastPainter()
{
  return this->LastPainter;
}

//-----------------------------------------------------------------------------
vtkAbstractContextBufferId *vtkContextScene::GetBufferId()
{
  return this->BufferId;
}

//-----------------------------------------------------------------------------
void vtkContextScene::SetTransform(vtkTransform2D* transform)
{
  if (this->Transform == transform)
    {
    return;
    }
  this->Transform->Delete();
  this->Transform = transform;
  this->Transform->Register(this);
}

//-----------------------------------------------------------------------------
vtkTransform2D* vtkContextScene::GetTransform()
{
  if (this->Transform)
    {
    return this->Transform;
    }
  else
    {
    this->Transform = vtkTransform2D::New();
    return this->Transform;
    }
}

//-----------------------------------------------------------------------------
void vtkContextScene::CheckForRepaint()
{
  // Called after interaction events - cause the scene to be repainted if any
  // events marked the scene as dirty.
  if (this->Renderer && this->Storage->IsDirty)
    {
    this->Renderer->GetRenderWindow()->Render();
    }
}

//-----------------------------------------------------------------------------
void vtkContextScene::ProcessSelectionEvent(vtkObject* caller, void* callData)
{
  cout << "ProcessSelectionEvent called! " << caller << "\t" << callData << endl;
  unsigned int *rect = reinterpret_cast<unsigned int *>(callData);
  cout << "Rect:";
  for (int i = 0; i < 5; ++i)
    {
    cout << "\t" << rect[i];
    }
  cout << endl;
}

// ----------------------------------------------------------------------------
void vtkContextScene::TestBufferIdSupport()
{
  if(!this->BufferIdSupportTested)
    {
    vtkOpenGLContextBufferId *b=vtkOpenGLContextBufferId::New();
    b->SetContext(static_cast<vtkOpenGLRenderWindow *>(
                    this->Renderer->GetRenderWindow()));
    this->BufferIdSupported=b->IsSupported();
    b->ReleaseGraphicsResources();
    b->Delete();
    this->BufferIdSupportTested=true;
    }
}

// ----------------------------------------------------------------------------
void vtkContextScene::UpdateBufferId()
{
  int lowerLeft[2];
  int width;
  int height;
  this->Renderer->GetTiledSizeAndOrigin(&width,&height,lowerLeft,
                                        lowerLeft+1);

  if(this->BufferId==0 || this->BufferIdDirty ||
     width!=this->BufferId->GetWidth() ||
     height!=this->BufferId->GetHeight())
    {
    if(this->BufferId==0)
      {
      vtkOpenGLContextBufferId *b=vtkOpenGLContextBufferId::New();
      this->BufferId=b;
      b->SetContext(static_cast<vtkOpenGLRenderWindow *>(
                      this->Renderer->GetRenderWindow()));
      }
    this->BufferId->SetWidth(width);
    this->BufferId->SetHeight(height);
    this->BufferId->Allocate();

    this->LastPainter->BufferIdModeBegin(this->BufferId);
    this->PaintIds();
    this->LastPainter->BufferIdModeEnd();

    this->BufferIdDirty=false;
    }
}

// ----------------------------------------------------------------------------
vtkIdType vtkContextScene::GetPickedItem(int x, int y)
{
  vtkIdType result = -1;
  this->TestBufferIdSupport();
  if (this->UseBufferId && this->BufferIdSupported)
    {
    this->UpdateBufferId();
    result=this->BufferId->GetPickedItem(x,y);
    }
  else
    {
    size_t i = this->Children->size();
    vtkContextMouseEvent &event = this->Storage->Event;
    for(vtkContextScenePrivate::const_reverse_iterator it =
        this->Children->rbegin(); it != this->Children->rend(); ++it, --i)
      {
      if ((*it)->Hit(event))
        {
        result = static_cast<vtkIdType>(i);
        break;
        }
      }
    }

  // Work-around for Qt bug under Linux (and maybe other platforms), 4.5.2
  // or 4.6.2 :
  // when the cursor leaves the window, Qt returns an extra mouse move event
  // with coordinates out of the window area. The problem is that the pixel
  // underneath is not owned by the OpenGL context, hence the bufferid contains
  // garbage (see OpenGL pixel ownership test).
  // As a workaround, any value out of the scope of
  // [-1,this->GetNumberOfItems()-1] is set to -1 (<=> no hit)

  if(result<-1 || result>=static_cast<vtkIdType>(this->GetNumberOfItems()))
    {
    result=-1;
    }

  assert("post: valid_result" && result>=-1 &&
         result<static_cast<vtkIdType>(this->GetNumberOfItems()));
  return result;
}

//-----------------------------------------------------------------------------
void vtkContextScene::MouseMoveEvent(int x, int y)
{
  int size = static_cast<int>(this->Children->size());
  vtkContextMouseEvent &event = this->Storage->Event;
  event.ScreenPos.Set(x, y);
  event.ScenePos.Set(x, y);
  event.Pos.Set(x, y);

  if(size != 0)
    {
    // Fire mouse enter and leave event prior to firing a mouse event.
    vtkIdType pickedItem=this->GetPickedItem(x,y);

    for (int i = size-1; i >= 0; --i)
      {
      if (this->Storage->itemMousePressCurrent == i)
        {
        // Don't send the mouse move event twice...
        continue;
        }

      if (i==pickedItem)
        {
        if (!this->Children->State[i] && this->Storage->itemMousePressCurrent < 0)
          {
          this->Children->State[i] = true;
          (*this->Children)[i]->MouseEnterEvent(event);
//          cout << "enter" << endl;
          }
        }
      else
        {
        if (this->Children->State[i])
          {
          this->Children->State[i] = false;
          (*this->Children)[i]->MouseLeaveEvent(event);
//          cout << "leave" << endl;
          }
        }
      }

    // Fire mouse move event regardless of where it occurred.

    // Check if there is a selected item that needs to receive a move event
    if (this->Storage->itemMousePressCurrent >= 0)
      {
      (*this->Children)[this->Storage->itemMousePressCurrent]
          ->MouseMoveEvent(event);
      }
    else
      {
      // Propagate mouse move events
      for (int i = size-1; i >= 0; --i)
        {
        if ((*this->Children)[i]->MouseMoveEvent(event))
          {
          break;
          }
        }
      }
    }

  // Update the last positions now
  event.LastScreenPos = event.ScreenPos;
  event.LastScenePos = event.ScenePos;
  event.LastPos = event.Pos;
}

//-----------------------------------------------------------------------------
void vtkContextScene::ButtonPressEvent(int button, int x, int y)
{
  int size = static_cast<int>(this->Children->size());
  vtkContextMouseEvent &event = this->Storage->Event;
  event.ScreenPos.Set(x, y);
  event.LastScreenPos = event.ScreenPos;
  event.ScenePos.Set(x, y);;
  event.LastScenePos = event.ScenePos;
  event.Pos.Set(x, y);
  event.LastPos = event.Pos;
  event.Button = button;
  for (int i = size-1; i >= 0; --i)
    {
    if ((*this->Children)[i]->Hit(event))
      {
      if ((*this->Children)[i]->MouseButtonPressEvent(event))
        {
        // The event was accepted - stop propagating
        this->Storage->itemMousePressCurrent = i;
        return;
        }
      }
    }
}

//-----------------------------------------------------------------------------
void vtkContextScene::ButtonReleaseEvent(int button, int x, int y)
{
  if (this->Storage->itemMousePressCurrent >= 0)
    {
    vtkContextMouseEvent &event = this->Storage->Event;
    event.ScreenPos.Set(x, y);
    event.ScenePos.Set(x, y);
    event.Pos.Set(x, y);
    event.Button = button;
    (*this->Children)[this->Storage->itemMousePressCurrent]
        ->MouseButtonReleaseEvent(event);
    this->Storage->itemMousePressCurrent = -1;
    }
  this->Storage->Event.Button = vtkContextMouseEvent::NO_BUTTON;
}

void vtkContextScene::MouseWheelEvent(int delta, int x, int y)
{
  int size = static_cast<int>(this->Children->size());
  vtkContextMouseEvent &event = this->Storage->Event;
  event.ScreenPos[0] = event.LastScreenPos[0] = x;
  event.ScreenPos[1] = event.LastScreenPos[1] = y;
  event.ScenePos[0] = event.LastScenePos[0] = x;
  event.ScenePos[1] = event.LastScenePos[1] = y;
  //event.Button = 1;
  for (int i = size-1; i >= 0; --i)
    {
    if ((*this->Children)[i]->Hit(event))
      {
      if ((*this->Children)[i]->MouseWheelEvent(event, delta))
        {
        // The event was accepted - stop propagating
        break;
        }
      }
    }

  if (this->Renderer)
    {
    this->Renderer->GetRenderWindow()->Render();
    }
}

//-----------------------------------------------------------------------------
inline void vtkContextScene::PerformTransform(vtkTransform2D *transform,
                                              vtkContextMouseEvent &mouse)
{
  if (transform)
    {
    transform->InverseTransformPoints(&mouse.ScenePos[0], &mouse.Pos[0], 1);
    }
  else
    {
    mouse.Pos[0] = mouse.ScenePos[0];
    mouse.Pos[1] = mouse.ScenePos[1];
    }
}

//-----------------------------------------------------------------------------
void vtkContextScene::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  // Print out the chart's geometry if it has been set
  os << indent << "Widthxheight: " << this->Geometry[0] << "\t" << this->Geometry[1]
     << endl;
}
