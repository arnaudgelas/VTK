/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkChart.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkChart.h"
#include "vtkAxis.h"
#include "vtkTransform2D.h"

#include "vtkAnnotationLink.h"
#include "vtkContextScene.h"
#include "vtkTextProperty.h"
#include "vtkObjectFactory.h"

//-----------------------------------------------------------------------------
vtkCxxSetObjectMacro(vtkChart, AnnotationLink, vtkAnnotationLink);

//-----------------------------------------------------------------------------
vtkChart::vtkChart()
{
  this->Geometry[0] = 0;
  this->Geometry[1] = 0;
  this->Point1[0] = 0;
  this->Point1[1] = 0;
  this->Point2[0] = 0;
  this->Point2[1] = 0;
  this->ShowLegend = false;
  this->Title = NULL;
  this->TitleProperties = vtkTextProperty::New();
  this->TitleProperties->SetJustificationToCentered();
  this->TitleProperties->SetColor(0.0, 0.0, 0.0);
  this->TitleProperties->SetFontSize(12);
  this->TitleProperties->SetFontFamilyToArial();
  this->AnnotationLink = NULL;
}

//-----------------------------------------------------------------------------
vtkChart::~vtkChart()
{
  this->SetTitle(NULL);
  this->TitleProperties->Delete();
  if (this->AnnotationLink)
    {
    this->AnnotationLink->Delete();
    }
}

//-----------------------------------------------------------------------------
vtkPlot * vtkChart::AddPlot(int)
{
  return NULL;
}

//-----------------------------------------------------------------------------
vtkIdType vtkChart::AddPlot(vtkPlot*)
{
  return -1;
}

//-----------------------------------------------------------------------------
bool vtkChart::RemovePlot(vtkIdType)
{
  return false;
}

//-----------------------------------------------------------------------------
bool vtkChart::RemovePlotInstance(vtkPlot* plot)
{
  if (plot)
    {
    vtkIdType numberOfPlots = this->GetNumberOfPlots();
    for (vtkIdType i = 0; i < numberOfPlots; ++i)
      {
      if (this->GetPlot(i) == plot)
        {
        return this->RemovePlot(i);
        }
      }
    }
  return false;
}

//-----------------------------------------------------------------------------
void vtkChart::ClearPlots()
{
}

//-----------------------------------------------------------------------------
vtkPlot* vtkChart::GetPlot(vtkIdType)
{
  return NULL;
}

//-----------------------------------------------------------------------------
vtkIdType vtkChart::GetNumberOfPlots()
{
  return 0;
}

//-----------------------------------------------------------------------------
vtkAxis* vtkChart::GetAxis(int)
{
  return NULL;
}

//-----------------------------------------------------------------------------
vtkIdType vtkChart::GetNumberOfAxes()
{
  return 0;
}

//-----------------------------------------------------------------------------
void vtkChart::RecalculateBounds()
{
  return;
}

//-----------------------------------------------------------------------------
bool vtkChart::CalculatePlotTransform(vtkAxis *x, vtkAxis *y,
                                      vtkTransform2D *transform)
{
  if (!x || !y || !transform)
    {
    vtkWarningMacro("Called with null arguments.");
    return false;
    }
  // Get the scale for the plot area from the x and y axes
  float *min = x->GetPoint1();
  float *max = x->GetPoint2();
  if (fabs(max[0] - min[0]) == 0.0f)
    {
    return false;
    }
  float xScale = (x->GetMaximum() - x->GetMinimum()) / (max[0] - min[0]);

  // Now the y axis
  min = y->GetPoint1();
  max = y->GetPoint2();
  if (fabs(max[1] - min[1]) == 0.0f)
    {
    return false;
    }
  float yScale = (y->GetMaximum() - y->GetMinimum()) / (max[1] - min[1]);

  transform->Identity();
  transform->Translate(this->Point1[0], this->Point1[1]);
  // Get the scale for the plot area from the x and y axes
  transform->Scale(1.0 / xScale, 1.0 / yScale);
  transform->Translate(-x->GetMinimum(), -y->GetMinimum());
  return true;
}

//-----------------------------------------------------------------------------
void vtkChart::SetBottomBorder(int border)
{
  this->Point1[1] = border >= 0 ? border : 0;
}

//-----------------------------------------------------------------------------
void vtkChart::SetTopBorder(int border)
{
 this->Point2[1] = border >=0 ?
                   this->Geometry[1] - border :
                   this->Geometry[1];
}

//-----------------------------------------------------------------------------
void vtkChart::SetLeftBorder(int border)
{
  this->Point1[0] = border >= 0 ? border : 0;
}

//-----------------------------------------------------------------------------
void vtkChart::SetRightBorder(int border)
{
  this->Point2[0] = border >=0 ?
                    this->Geometry[0] - border :
                    this->Geometry[0];
}

//-----------------------------------------------------------------------------
void vtkChart::SetBorders(int left, int bottom, int right, int top)
{
  this->SetLeftBorder(left);
  this->SetRightBorder(right);
  this->SetTopBorder(top);
  this->SetBottomBorder(bottom);
}

//-----------------------------------------------------------------------------
void vtkChart::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  // Print out the chart's geometry if it has been set
  os << indent << "Point1: " << this->Point1[0] << "\t" << this->Point1[1]
     << endl;
  os << indent << "Point2: " << this->Point2[0] << "\t" << this->Point2[1]
     << endl;
  os << indent << "Width: " << this->Geometry[0] << endl
     << indent << "Height: " << this->Geometry[1] << endl;
}
