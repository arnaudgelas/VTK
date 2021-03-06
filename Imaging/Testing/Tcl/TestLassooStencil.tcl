package require vtk


# A script to test the vtkLassooStencilSource


vtkPNGReader reader
reader SetDataSpacing 0.8 0.8 1.5
reader SetDataOrigin  0.0 0.0 0.0
reader SetFileName "$VTK_DATA_ROOT/Data/fullhead15.png"

vtkImageShiftScale shiftScale
shiftScale SetInputConnection [reader GetOutputPort]
shiftScale SetScale 0.2

vtkPoints points1
points1 InsertNextPoint 80 50 0
points1 InsertNextPoint 100 90 0
points1 InsertNextPoint 200 50 0
points1 InsertNextPoint 230 100 0
points1 InsertNextPoint 150 170 0
points1 InsertNextPoint 110 170 0
points1 InsertNextPoint 80 50 0

vtkPoints points2
points2 InsertNextPoint 80 50 0
points2 InsertNextPoint 100 90 0
points2 InsertNextPoint 200 50 0
points2 InsertNextPoint 230 100 0
points2 InsertNextPoint 150 170 0
points2 InsertNextPoint 110 170 0

vtkLassooStencilSource roiStencil1
roiStencil1 SetShapeToPolygon
roiStencil1 SetSlicePoints 0 points1
roiStencil1 SetInformationInput [reader GetOutput]

vtkLassooStencilSource roiStencil2
roiStencil2 SetShapeToPolygon
roiStencil2 SetPoints points2
roiStencil2 SetInformationInput [reader GetOutput]

vtkLassooStencilSource roiStencil3
roiStencil3 SetShapeToSpline
roiStencil3 SetPoints points1
roiStencil3 SetInformationInput [reader GetOutput]

vtkLassooStencilSource roiStencil4
roiStencil4 SetShapeToSpline
roiStencil4 SetSlicePoints 0 points2
roiStencil4 SetInformationInput [reader GetOutput]

vtkImageStencil stencil1
stencil1 SetInputConnection [reader GetOutputPort]
stencil1 SetBackgroundInput [shiftScale GetOutput]
stencil1 SetStencil [roiStencil1 GetOutput]

vtkImageStencil stencil2
stencil2 SetInputConnection [reader GetOutputPort]
stencil2 SetBackgroundInput [shiftScale GetOutput]
stencil2 SetStencil [roiStencil2 GetOutput]

vtkImageStencil stencil3
stencil3 SetInputConnection [reader GetOutputPort]
stencil3 SetBackgroundInput [shiftScale GetOutput]
stencil3 SetStencil [roiStencil3 GetOutput]

vtkImageStencil stencil4
stencil4 SetInputConnection [reader GetOutputPort]
stencil4 SetBackgroundInput [shiftScale GetOutput]
stencil4 SetStencil [roiStencil4 GetOutput]

vtkImageMapper mapper1
  mapper1 SetInputConnection [stencil1 GetOutputPort]
  mapper1 SetColorWindow 2000
  mapper1 SetColorLevel 1000
  mapper1 SetZSlice 0

vtkImageMapper mapper2
  mapper2 SetInputConnection [stencil2 GetOutputPort]
  mapper2 SetColorWindow 2000
  mapper2 SetColorLevel 1000
  mapper2 SetZSlice 0

vtkImageMapper mapper3
  mapper3 SetInputConnection [stencil3 GetOutputPort]
  mapper3 SetColorWindow 2000
  mapper3 SetColorLevel 1000
  mapper3 SetZSlice 0

vtkImageMapper mapper4
  mapper4 SetInputConnection [stencil4 GetOutputPort]
  mapper4 SetColorWindow 2000
  mapper4 SetColorLevel 1000
  mapper4 SetZSlice 0

vtkActor2D actor1
  actor1 SetMapper mapper1

vtkActor2D actor2
  actor2 SetMapper mapper2

vtkActor2D actor3
  actor3 SetMapper mapper3

vtkActor2D actor4
  actor4 SetMapper mapper4

vtkRenderer imager1
  imager1 AddActor2D actor1
  imager1 SetViewport 0.5 0.0 1.0 0.5

vtkRenderer imager2
  imager2 AddActor2D actor2
  imager2 SetViewport 0.0 0.0 0.5 0.5

vtkRenderer imager3
  imager3 AddActor2D actor3
  imager3 SetViewport 0.5 0.5 1.0 1.0

vtkRenderer imager4
  imager4 AddActor2D actor4
  imager4 SetViewport 0.0 0.5 0.5 1.0

vtkRenderWindow imgWin
  imgWin AddRenderer imager1
  imgWin AddRenderer imager2
  imgWin AddRenderer imager3
  imgWin AddRenderer imager4
  imgWin SetSize 512 512

imgWin Render
