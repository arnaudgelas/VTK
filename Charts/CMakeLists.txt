SET(KIT Charts)
SET(UKIT CHARTS)

SET(KIT_TCL_LIBS vtkHybridTCL vtkViewsTCL ${VTK_TK_LIBRARIES})
SET(KIT_PYTHON_LIBS vtkHybridPythonD vtkViewsPythonD)
SET(KIT_JAVA_LIBS vtkHybridJava vtkViewsJava)

IF (JAVA_AWT_LIBRARY)
  SET(KIT_JAVA_LIBS ${KIT_JAVA_LIBS} ${JAVA_AWT_LIBRARY})
ENDIF (JAVA_AWT_LIBRARY)

SET(KIT_INTERFACE_LIBRARIES vtkHybrid vtkViews)
SET(KIT_LIBS vtkIO vtkftgl
  ${VTK_FREETYPE_LIBRARIES}
)

SET(Kit_SRCS
  vtk2DHistogramItem.cxx
  vtkAbstractContextBufferId.cxx
  vtkAbstractContextItem.cxx
  vtkAxis.cxx
  vtkBlockItem.cxx
  vtkBrush.cxx
  vtkChart.cxx
  vtkChartLegend.cxx
  vtkChart2DHistogram.cxx
  vtkChartParallelCoordinates.cxx
  vtkChartXY.cxx
  vtkChartPie.cxx
  vtkColorLegend.cxx
  vtkPlotPie.cxx
  vtkColor.h
  vtkColorSeries.cxx
  vtkColorTransferFunctionItem.cxx
  vtkCompositeTransferFunctionItem.cxx
  vtkContext2D.cxx
  vtkContextActor.cxx
  vtkContextBufferId.cxx
  vtkContextClip.cxx
  vtkContextDevice2D.cxx
  vtkContextItem.cxx
  vtkContextMapper2D.cxx
  vtkContextMouseEvent.h
  vtkContextScene.cxx
  vtkContextTransform.cxx
  vtkContextView.cxx
  vtkControlPointsItem.cxx
  vtkImageItem.cxx
  vtkLookupTableItem.cxx
  vtkOpenGLContextBufferId.cxx
  vtkOpenGLContextDevice2D.cxx
  vtkPen.cxx
  vtkPiecewiseControlPointsItem.cxx
  vtkPiecewiseFunctionItem.cxx
  vtkPlot.cxx
  vtkPlotBar.cxx
  vtkPlotGrid.cxx
  vtkPlotLine.cxx
  vtkPlotStacked.cxx
  vtkPlotParallelCoordinates.cxx
  vtkPlotPoints.cxx
  vtkScalarsToColorsItem.cxx
  vtkTooltipItem.cxx
  vtkVector.h
  )

SET_SOURCE_FILES_PROPERTIES(
  vtkColor
  vtkContextMouseEvent
  vtkVector
  HEADER_FILE_ONLY
)

SET_SOURCE_FILES_PROPERTIES(
  vtkAbstractContextBufferId
  vtkAbstractContextItem
  vtkChart
  vtkColor
  vtkContextDevice2D
  vtkContextItem
  vtkContextMapper2D
  vtkControlPointsItem
  vtkPlot
  vtkScalarsToColorsItem
  vtkVector
  ABSTRACT
  )

SET_SOURCE_FILES_PROPERTIES(
  vtkAbstractContextBufferId
  vtkColor
  vtkContextBufferId
  vtkContextMouseEvent
  vtkChartColors
  vtkOpenGLContextBufferId
  vtkOpenGLContextDevice2D
  vtkVector
  WRAP_EXCLUDE
)

# Need to be able to include QApplication if Qt is used for runtime checks
IF(VTK_USE_QT)
  SET(QT_DONT_USE_QTGUI)
  INCLUDE(${QT_USE_FILE})
  IF (APPLE)
    LIST (APPEND KIT_INTERFACE_LIBRARIES ${QT_LIBRARIES})
  ELSE (APPLE)
    LIST (APPEND KIT_LIBS ${QT_LIBRARIES})
  ENDIF (APPLE)
ENDIF(VTK_USE_QT)

#-----------------------------------------------------------------------------
# Include CMake code common to all kits.
INCLUDE(${VTK_CMAKE_DIR}/KitCommonBlock.cmake)
#-----------------------------------------------------------------------------

#TARGET_LINK_LIBRARIES(vtkCharts ${QT_LIBRARIES})

