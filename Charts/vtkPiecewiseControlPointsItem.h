/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkPiecewiseControlPointsItem.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef __vtkPiecewiseControlPointsItem_h
#define __vtkPiecewiseControlPointsItem_h

#include "vtkControlPointsItem.h"

class vtkPiecewiseFunction;

class VTK_CHARTS_EXPORT vtkPiecewiseControlPointsItem: public vtkControlPointsItem
{
public:
  static vtkPiecewiseControlPointsItem* New();
  vtkTypeMacro(vtkPiecewiseControlPointsItem, vtkControlPointsItem);
  virtual void PrintSelf(ostream &os, vtkIndent indent);

  void SetPiecewiseFunction(vtkPiecewiseFunction* function);
  vtkGetObjectMacro(PiecewiseFunction, vtkPiecewiseFunction);

protected:
  vtkPiecewiseControlPointsItem();
  virtual ~vtkPiecewiseControlPointsItem();

  virtual void ComputePoints();

  // Description:
  // Returns true if the supplied x, y coordinate is inside the item.
  virtual bool Hit(const vtkContextMouseEvent &mouse);

  // Description:
  // Mouse move event.
  virtual bool MouseMoveEvent(const vtkContextMouseEvent &mouse);

  // Description:
  // Mouse button down event.
  virtual bool MouseButtonPressEvent(const vtkContextMouseEvent &mouse);

  // Description:
  // Mouse button release event.
  virtual bool MouseButtonReleaseEvent(const vtkContextMouseEvent &mouse);

  vtkPiecewiseFunction* PiecewiseFunction;

  float     ButtonPressPosition[2];
  vtkIdType MouseOver;

private:
  vtkPiecewiseControlPointsItem(const vtkPiecewiseControlPointsItem &); // Not implemented.
  void operator=(const vtkPiecewiseControlPointsItem &);   // Not implemented.
};

#endif
