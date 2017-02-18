/***************************************************************************
 *   Copyright (c) 2017 Kustaa Nyholm  <kustaa.nyholm@sparetimelabs.com>   *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"
#ifndef _PreComp_
# include <float.h>
# ifdef FC_OS_WIN32
#  include <windows.h>
# endif
# ifdef FC_OS_MACOSX
# include <OpenGL/gl.h>
# else
# include <GL/gl.h>
# endif
# include <Inventor/SbBox.h>
# include <Inventor/actions/SoGetBoundingBoxAction.h>
# include <Inventor/actions/SoGetMatrixAction.h>
# include <Inventor/actions/SoHandleEventAction.h>
# include <Inventor/actions/SoToVRML2Action.h>
# include <Inventor/actions/SoWriteAction.h>
# include <Inventor/elements/SoViewportRegionElement.h>
# include <Inventor/manips/SoClipPlaneManip.h>
# include <Inventor/nodes/SoBaseColor.h>
# include <Inventor/nodes/SoCallback.h>
# include <Inventor/nodes/SoCoordinate3.h>
# include <Inventor/nodes/SoCube.h>
# include <Inventor/nodes/SoDirectionalLight.h>
# include <Inventor/nodes/SoEventCallback.h>
# include <Inventor/nodes/SoFaceSet.h>
# include <Inventor/nodes/SoImage.h>
# include <Inventor/nodes/SoIndexedFaceSet.h>
# include <Inventor/nodes/SoLightModel.h>
# include <Inventor/nodes/SoLocateHighlight.h>
# include <Inventor/nodes/SoMaterial.h>
# include <Inventor/nodes/SoMaterialBinding.h>
# include <Inventor/nodes/SoOrthographicCamera.h>
# include <Inventor/nodes/SoPerspectiveCamera.h>
# include <Inventor/nodes/SoRotationXYZ.h>
# include <Inventor/nodes/SoSeparator.h>
# include <Inventor/nodes/SoShapeHints.h>
# include <Inventor/nodes/SoSwitch.h>
# include <Inventor/nodes/SoTransform.h>
# include <Inventor/nodes/SoTranslation.h>
# include <Inventor/nodes/SoSelection.h>
# include <Inventor/nodes/SoText2.h>
# include <Inventor/actions/SoBoxHighlightRenderAction.h>
# include <Inventor/events/SoEvent.h>
# include <Inventor/events/SoKeyboardEvent.h>
# include <Inventor/events/SoLocation2Event.h>
# include <Inventor/events/SoMotion3Event.h>
# include <Inventor/events/SoMouseButtonEvent.h>
# include <Inventor/actions/SoRayPickAction.h>
# include <Inventor/projectors/SbSphereSheetProjector.h>
# include <Inventor/SoOffscreenRenderer.h>
# include <Inventor/SoPickedPoint.h>
# include <Inventor/VRMLnodes/SoVRMLGroup.h>
# include <QEventLoop>
# include <QGLFramebufferObject>
# include <QGLPixelBuffer>
# include <QKeyEvent>
# include <QWheelEvent>
# include <QMessageBox>
# include <QTimer>
# include <QStatusBar>
# include <QBitmap>
# include <QMimeData>
#endif

#include <sstream>
#include <Base/Console.h>
#include <Base/Stream.h>
#include <Base/FileInfo.h>
#include <Base/Sequencer.h>
#include <Base/Tools.h>
#include <Base/UnitsApi.h>

#include "View3DInventorViewer.h"

#include "NaviCube.h"

#include <QCursor>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QImage>
#include <QPainterPath>
//#include <OpenGL/glu.h>
#include <Eigen/Dense>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>

using namespace Eigen;
using namespace std;
using namespace Gui;


class Face {
public:
	int m_FirstVertex;
	int m_VertexCount;
	GLuint m_TextureId;
	QColor m_Color;
	int m_PickId;
	GLuint m_PickTextureId;
	Face(
		 int firstVertex,
		 int vertexCount,
		 GLuint textureId,
		 int pickId,
		 GLuint pickTextureId,
		 const QColor& color
		)
	{
		m_FirstVertex = firstVertex;
		m_VertexCount = vertexCount;
		m_TextureId = textureId;
		m_PickId = pickId;
		m_PickTextureId = pickTextureId;
		m_Color = color;
	}
};

class NaviCubeImplementation {
public:
	NaviCubeImplementation(Gui::View3DInventorViewer*);
	virtual ~ NaviCubeImplementation();
	void drawNaviCube();

	bool processSoEvent(const SoEvent* ev);
private:
	bool mousePressed(const SoMouseButtonEvent*);
	bool mouseReleased(const SoMouseButtonEvent*);
	bool mouseMoved(const SoEvent*);

	void handleResize();
	void handleMenu();

	void setHilite(int);
	int pickFace(SbVec2s);

	void initNaviCube(QGLWidget*);
	void addFace(const Vector3f&, const Vector3f&, int, int, int, int);

	GLuint createCubeFaceTex(QGLWidget*, float, float, char*);
	GLuint createButtonTex(QGLWidget*, int);
	GLuint createMenuTex(QGLWidget*,bool);

	void setView(float ,float );
	void rotateView(int ,float );

	bool inDragZone(const SbVec2s& pos);
	QString str(char* str);
	char* enum2str(int);
public:
	enum { //
		TEX_FRONT = 1, // 0 is reserved for 'nothing picked'
		TEX_REAR,
		TEX_TOP,
		TEX_BOTTOM,
		TEX_LEFT,
		TEX_RIGHT,
		TEX_BACK_FACE,
		TEX_FRONT_FACE,
		TEX_CORNER_FACE,
		TEX_CORNER_BOTTOM_RIGHT_REAR,
		TEX_CORNER_BOTTOM_FRONT_RIGHT,
		TEX_CORNER_BOTTOM_LEFT_FRONT,
		TEX_CORNER_BOTTOM_REAR_LEFT,
		TEX_CORNER_TOP_RIGHT_FRONT,
		TEX_CORNER_TOP_FRONT_LEFT,
		TEX_CORNER_TOP_LEFT_REAR,
		TEX_CORNER_TOP_REAR_RIGHT,
		TEX_ARROW_NORTH,
		TEX_ARROW_SOUTH,
		TEX_ARROW_EAST,
		TEX_ARROW_WEST,
		TEX_ARROW_RIGHT,
		TEX_ARROW_LEFT,
		TEX_VIEW_MENU_ICON,
		TEX_VIEW_MENU
	};
	enum {
		DIR_UP,DIR_RIGHT,DIR_OUT
	};
	Gui::View3DInventorViewer* m_View3DInventorViewer;
	void drawNaviCube(bool picking);

	int m_CubeWidgetSize = 0;
	int m_CubeWidgetPosX = 0;
	int m_CubeWidgetPosY = 0;
	int m_PrevWidth = 0;
	int m_PrevHeight = 0;
	QColor m_HiliteColor;
	QColor m_ButtonColor;
	QColor m_FrontFaceColor;
	QColor m_BackFaceColor;
	int m_HiliteId = 0;
	bool m_MouseDown = false;
	bool m_Dragging = false;
	bool m_MightDrag = false;

	QGLFramebufferObject* m_PickingFramebuffer;

	bool m_NaviCubeInitialised = false;

	vector<GLubyte> m_IndexArray;
	vector<Vector2f> m_TextureCoordArray;
	vector<Vector3f> m_VertexArray;
	map<int,GLuint> m_Textures;
	vector<Face*> m_Faces;
	vector<int> m_Buttons;
};

NaviCube::NaviCube(Gui::View3DInventorViewer* viewer) {
	m_NaviCubeImplementation = new NaviCubeImplementation(viewer);

}

NaviCube::~NaviCube() {
	delete m_NaviCubeImplementation;
}

void NaviCube::drawNaviCube() {
	m_NaviCubeImplementation->drawNaviCube();
}

bool NaviCube::processSoEvent(const SoEvent* ev) {
	return m_NaviCubeImplementation->processSoEvent(ev);
}

NaviCubeImplementation::NaviCubeImplementation(
		Gui::View3DInventorViewer* viewer) {
	m_View3DInventorViewer = viewer;
	m_FrontFaceColor = QColor(255,255,255,128);
	m_BackFaceColor = QColor(226,233,239);
	m_HiliteColor = QColor(170,226,247);
	m_ButtonColor = QColor(226,233,239);
	m_PickingFramebuffer = NULL;
	m_CubeWidgetSize = 150;
}

NaviCubeImplementation::~NaviCubeImplementation() {
	if (m_PickingFramebuffer)
		delete m_PickingFramebuffer;
	for (vector<Face*>::iterator f = m_Faces.begin(); f != m_Faces.end(); f++)
		delete *f;
}

char* NaviCubeImplementation::enum2str(int e) {
	switch (e) {
	default : return "???";
	case TEX_FRONT : return "TEX_FRONT";
	case TEX_REAR: return "TEX_REAR";
	case TEX_TOP: return "TEX_TOP";
	case TEX_BOTTOM: return "TEX_BOTTOM";
	case TEX_RIGHT : return "TEX_RIGHT";
	case TEX_LEFT: return "TEX_LEFT";
	case TEX_BACK_FACE: return "TEX_BACK_FACE";
	case TEX_FRONT_FACE: return "TEX_FRONT_FACE";
	case TEX_CORNER_FACE: return "TEX_CORNER_FACE";
	case TEX_CORNER_BOTTOM_RIGHT_REAR: return "TEX_CORNER_BOTTOM_RIGHT_REAR";
	case TEX_CORNER_BOTTOM_FRONT_RIGHT: return "TEX_CORNER_BOTTOM_FRONT_RIGHT";
	case TEX_CORNER_BOTTOM_LEFT_FRONT: return "TEX_CORNER_BOTTOM_LEFT_FRONT";
	case TEX_CORNER_BOTTOM_REAR_LEFT: return "TEX_CORNER_BOTTOM_REAR_LEFT";
	case TEX_CORNER_TOP_RIGHT_FRONT: return "TEX_CORNER_TOP_RIGHT_FRONT";
	case TEX_CORNER_TOP_FRONT_LEFT: return "TEX_CORNER_TOP_FRONT_LEFT";
	case TEX_CORNER_TOP_LEFT_REAR: return "TEX_CORNER_TOP_LEFT_REAR";
	case TEX_CORNER_TOP_REAR_RIGHT: return "TEX_CORNER_TOP_REAR_RIGHT";
	case TEX_ARROW_NORTH: return "TEX_ARROW_NORTH";
	case TEX_ARROW_SOUTH: return "TEX_ARROW_SOUTH";
	case TEX_ARROW_EAST: return "TEX_ARROW_EAST";
	case TEX_ARROW_WEST: return "TEX_ARROW_WEST";
	case TEX_ARROW_RIGHT: return "TEX_ARROW_RIGHT";
	case TEX_ARROW_LEFT: return "TEX_ARROW_LEFT";
	case TEX_VIEW_MENU_ICON : return "TEX_VIEW_MENU_ICON";
	case TEX_VIEW_MENU: return "TEX_VIEW_MENU";
	}
}
GLuint NaviCubeImplementation::createCubeFaceTex(QGLWidget* gl, float gap, float radius, char* imageName) {
	int texSize = m_CubeWidgetSize;
	int gapi = texSize * gap;
	int radiusi = texSize * radius;
	QImage image(texSize, texSize, QImage::Format_ARGB32);
	image.fill(qRgba(255, 255, 255, 0));
	QPainter paint;
	paint.begin(&image);

	QPainterPath path;
	path.addRoundedRect(QRectF(gapi, gapi, texSize - 2 * gapi, texSize - 2 * gapi), radiusi, radiusi);
	paint.fillPath(path, Qt::white);

	paint.setPen(Qt::black);
	QFont sansFont(str("Helvetica"), 0.1875 * texSize);
	paint.setFont(sansFont);
	paint.drawText(QRect(0, 0, texSize, texSize), Qt::AlignCenter,str(imageName));

	paint.end();
	//image.save(str((imageName))+str(".png"));

	return gl->bindTexture(image);
}


GLuint NaviCubeImplementation::createButtonTex(QGLWidget* gl, int button) {
	int texSize = m_CubeWidgetSize;
	QImage image(texSize, texSize, QImage::Format_ARGB32);
	image.fill(qRgba(255, 255, 255, 0));
	QPainter painter;
	painter.begin(&image);

	QTransform transform;
	transform.translate(texSize / 2, texSize / 2);
	transform.scale(texSize / 2, texSize / 2);
	painter.setTransform(transform);

	QPainterPath path;

	float as1 = 0.12; // arrow size
	float as3 = as1 / 3;

	switch (button) {
	default:
		break;
	case TEX_ARROW_RIGHT:
	case TEX_ARROW_LEFT: {
		QRectF r(-1.00, -1.00, 2.00, 2.00);
		QRectF r0(r);
		r.adjust(as3, as3, -as3, -as3);
		QRectF r1(r);
		r.adjust(as3, as3, -as3, -as3);
		QRectF r2(r);
		r.adjust(as3, as3, -as3, -as3);
		QRectF r3(r);
		r.adjust(as3, as3, -as3, -as3);
		QRectF r4(r);

		float a0 = 72;
		float a1 = 45;
		float a2 = 38;

		if (TEX_ARROW_LEFT == button) {
			a0 = 180 - a0;
			a1 = 180 - a1;
			a2 = 180 - a2;
		}

		path.arcMoveTo(r0, a1);
		QPointF p0 = path.currentPosition();

		path.arcMoveTo(r2, a2);
		QPointF p1 = path.currentPosition();

		path.arcMoveTo(r4, a1);
		QPointF p2 = path.currentPosition();

		path.arcMoveTo(r1, a0);
		path.arcTo(r1, a0, -(a0 - a1));
		path.lineTo(p0);
		path.lineTo(p1);
		path.lineTo(p2);
		path.arcTo(r3, a1, +(a0 - a1));
		break;
	}
	case TEX_ARROW_EAST: {
		path.moveTo(1, 0);
		path.lineTo(1 - as1, +as1);
		path.lineTo(1 - as1, -as1);
		break;
	}
	case TEX_ARROW_WEST: {
		path.moveTo(-1, 0);
		path.lineTo(-1 + as1, -as1);
		path.lineTo(-1 + as1, +as1);
		break;
	}
	case TEX_ARROW_SOUTH: {
		path.moveTo(0, 1);
		path.lineTo(-as1,1 - as1);
		path.lineTo(+as1,1 - as1);
		break;
	}
	case TEX_ARROW_NORTH: {
		path.moveTo(0, -1);
		path.lineTo(+as1,-1 + as1);
		path.lineTo(-as1,-1 + as1);
		break;
	}
	}

	painter.fillPath(path, Qt::white);

	painter.end();
	//image.save(str(enum2str(button))+str(".png"));

	return gl->bindTexture(image);
}

GLuint NaviCubeImplementation::createMenuTex(QGLWidget* gl, bool forPicking) {
	int texSize = m_CubeWidgetSize;
	QImage image(texSize, texSize, QImage::Format_ARGB32);
	image.fill(qRgba(0, 0, 0, 0));
	QPainter painter;
	painter.begin(&image);

	QTransform transform;
	transform.translate(texSize * 12 / 16, texSize * 13 / 16);
	transform.scale(texSize/200.0,texSize/200.0); // 200 == size at which this was designed
	painter.setTransform(transform);

	QColor w=Qt::white;

	QPainterPath path;

	// top
	path.moveTo(0,0);
	path.lineTo(15,5);
	path.lineTo(0,10);
	path.lineTo(-15,5);

	painter.fillPath(path, forPicking ? w : QColor(240,240,240));

	// left
	QPainterPath path2;
	path2.lineTo(0,10);
	path2.lineTo(-15,5);
	path2.lineTo(-15,25);
	path2.lineTo(0,30);
	painter.fillPath(path2, forPicking ? w : QColor(190,190,190));

	// right
	QPainterPath path3;
	path3.lineTo(0,10);
	path3.lineTo(15,5);
	path3.lineTo(15,25);
	path3.lineTo(0,30);
	painter.fillPath(path3, forPicking ? w : QColor(220,220,220));

	// outline
	QPainterPath path4;
	path4.moveTo(0,0);
	path4.lineTo(15,5);
	path4.lineTo(15,25);
	path4.lineTo(0,30);
	path4.lineTo(-15,25);
	path4.lineTo(-15,5);
	path4.lineTo(0,0);
	painter.strokePath(path4, forPicking ? w : QColor(128,128,128));

	// menu triangle
	QPainterPath path5;
	path5.moveTo(20,10);
	path5.lineTo(40,10);
	path5.lineTo(30,20);
	path5.lineTo(20,10);
	painter.fillPath(path5, forPicking ? w : QColor(64,64,64));

	painter.end();

	//image.save(str("menuimage.png"));

	return gl->bindTexture(image);
}

void NaviCubeImplementation::addFace(const Vector3f& x, const Vector3f& z, int frontTex, int backTex, int pickTex, int pickId) {
	Vector3f y = x.cross(-z);
	y = y / y.norm() * x.norm();

	int t = m_VertexArray.size();

	m_VertexArray.push_back(z - x - y);
	m_TextureCoordArray.push_back(Vector2f(0, 0));
	m_VertexArray.push_back(z + x - y);
	m_TextureCoordArray.push_back(Vector2f(1, 0));
	m_VertexArray.push_back(z + x + y);
	m_TextureCoordArray.push_back(Vector2f(1, 1));
	m_VertexArray.push_back(z - x + y);
	m_TextureCoordArray.push_back(Vector2f(0, 1));

	Face* ff = new Face(
		m_IndexArray.size(),
		4,
		m_Textures[frontTex],
		pickId,
		m_Textures[pickTex],
		m_FrontFaceColor);

	m_Faces.push_back(ff);

	for (int i = 0; i < 4; i++)
		m_IndexArray.push_back(t + i);

	Face* bf = new Face(
		m_IndexArray.size(),
		4,
		m_Textures[backTex],
		pickId,
		m_Textures[backTex],
		m_BackFaceColor);

	m_Faces.push_back(bf);

	for (int i = 0; i < 4; i++)
		m_IndexArray.push_back(t + 4 - 1 - i);
}

void NaviCubeImplementation::initNaviCube(QGLWidget* gl) {
	Vector3f x(1, 0, 0);
	Vector3f y(0, 1, 0);
	Vector3f z(0, 0, 1);

	float cs, sn;
	cs = cos(90 * M_PI / 180);
	sn = sin(90 * M_PI / 180);
	Matrix3f r90x;
	r90x << 1, 0, 0,
			0, cs, -sn,
			0, sn, cs;

	Matrix3f r90z;
	r90z << cs, sn, 0,
			-sn, cs, 0,
			0, 0, 1;

	cs = cos(45 * M_PI / 180);
	sn = sin(45 * M_PI / 180);
	Matrix3f r45z;
	r45z << cs, sn, 0,
			-sn, cs, 0,
			0, 0, 1;

	cs = cos(atan(sqrt(2)));
	sn = sin(atan(sqrt(2)));
	Matrix3f r45x;
	r45x << 1, 0, 0,
			0, cs, -sn,
			0, sn, cs;

	m_Textures[TEX_CORNER_FACE] = createCubeFaceTex(gl, 0, 0.5, "");
	m_Textures[TEX_BACK_FACE] = createCubeFaceTex(gl, 0.02, 0.3, "");

	float gap = 0.12;
	float radius = 0.12;

	m_Textures[TEX_FRONT] = createCubeFaceTex(gl, gap, radius, "Front");
	m_Textures[TEX_REAR] = createCubeFaceTex(gl, gap, radius, "Rear");
	m_Textures[TEX_TOP] = createCubeFaceTex(gl, gap, radius, "Top");
	m_Textures[TEX_BOTTOM] = createCubeFaceTex(gl, gap, radius, "Bottom");
	m_Textures[TEX_RIGHT] = createCubeFaceTex(gl, gap, radius, "Right");
	m_Textures[TEX_LEFT] = createCubeFaceTex(gl, gap, radius, "Left");
	m_Textures[TEX_FRONT_FACE] = createCubeFaceTex(gl, gap, radius, "");
	m_Textures[TEX_ARROW_NORTH] = createButtonTex(gl, TEX_ARROW_NORTH);
	m_Textures[TEX_ARROW_SOUTH] = createButtonTex(gl, TEX_ARROW_SOUTH);
	m_Textures[TEX_ARROW_EAST] = createButtonTex(gl, TEX_ARROW_EAST);
	m_Textures[TEX_ARROW_WEST] = createButtonTex(gl, TEX_ARROW_WEST);
	m_Textures[TEX_ARROW_LEFT] = createButtonTex(gl, TEX_ARROW_LEFT);
	m_Textures[TEX_ARROW_RIGHT] = createButtonTex(gl, TEX_ARROW_RIGHT);
	m_Textures[TEX_VIEW_MENU_ICON] = createMenuTex(gl,false);
	m_Textures[TEX_VIEW_MENU] = createMenuTex(gl,true);

	addFace(x, z, TEX_TOP, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_TOP);
	x = r90x * x;
	z = r90x * z;
	addFace(x, z, TEX_FRONT, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_FRONT);
	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_LEFT, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_LEFT);
	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_REAR, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_REAR);
	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_RIGHT, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_RIGHT);
	x = r90x * r90z * x;
	z = r90x * r90z * z;
	addFace(x, z, TEX_BOTTOM, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_BOTTOM);

	z = r45z * r45x * z;
	x = r45z * r45x * x;

	x *= 0.25; // corner face size
	z *= 1.45; // corner face position

	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_BOTTOM_RIGHT_REAR);

	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_BOTTOM_FRONT_RIGHT);

	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_BOTTOM_LEFT_FRONT);

	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_BOTTOM_REAR_LEFT);

	x = r90x * r90x * r90z * x;
	z = r90x * r90x * r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_TOP_RIGHT_FRONT);

	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_TOP_FRONT_LEFT);

	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_TOP_LEFT_REAR);

	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_TOP_REAR_RIGHT);

	m_Buttons.push_back(TEX_ARROW_NORTH);
	m_Buttons.push_back(TEX_ARROW_SOUTH);
	m_Buttons.push_back(TEX_ARROW_EAST);
	m_Buttons.push_back(TEX_ARROW_WEST);
	m_Buttons.push_back(TEX_ARROW_LEFT);
	m_Buttons.push_back(TEX_ARROW_RIGHT);

	m_PickingFramebuffer = new QGLFramebufferObject(m_CubeWidgetSize, m_CubeWidgetSize, QGLFramebufferObject::CombinedDepthStencil);
}

void NaviCubeImplementation::drawNaviCube() {
	glViewport(m_CubeWidgetPosX-m_CubeWidgetSize/2, m_CubeWidgetPosY-m_CubeWidgetSize/2, m_CubeWidgetSize, m_CubeWidgetSize);
	drawNaviCube(false);
}

void NaviCubeImplementation::handleResize() {
	SbVec2s view = m_View3DInventorViewer->getSoRenderManager()->getSize();
	if ((m_PrevWidth != view[0]) || (m_PrevHeight != view[1])) {
		if ((m_PrevWidth > 0) && (m_PrevHeight > 0)) {
			// maintain position relative to closest edge
			if (m_CubeWidgetPosX > m_PrevWidth / 2)
				m_CubeWidgetPosX = view[0] - (m_PrevWidth -m_CubeWidgetPosX);
			if (m_CubeWidgetPosY > m_PrevHeight / 2)
				m_CubeWidgetPosY = view[1] - (m_PrevHeight - m_CubeWidgetPosY);
		}
		else { // initial position
			m_CubeWidgetPosX = view[0] - m_CubeWidgetSize*1.1 / 2;
			m_CubeWidgetPosY = view[1] - m_CubeWidgetSize*1.1 / 2;
		}
		m_PrevWidth = view[0];
		m_PrevHeight = view[1];
		m_View3DInventorViewer->getSoRenderManager()->scheduleRedraw();

	}

}

void NaviCubeImplementation::drawNaviCube(bool pickMode) {
	// initilizes stuff here when we actually have a context
	if (!m_NaviCubeInitialised) {
		QGLWidget* gl = static_cast<QGLWidget*>(m_View3DInventorViewer->viewport());
		if (gl == NULL)
			return;
		initNaviCube(gl);
		m_NaviCubeInitialised = true;
	}

	SoCamera* cam = m_View3DInventorViewer->getSoRenderManager()->getCamera();

	if (!cam)
		return;

	handleResize();

	// Store GL state.
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	GLfloat depthrange[2];
	glGetFloatv(GL_DEPTH_RANGE, depthrange);
	GLdouble projectionmatrix[16];
	glGetDoublev(GL_PROJECTION_MATRIX, projectionmatrix);

	glDepthMask(GL_TRUE);
	glDepthRange(0.0, 1.0);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glDisable(GL_LIGHTING);
	//glDisable(GL_BLEND);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//glTexEnvf(GL_TEXTURE_2D, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthMask(GL_TRUE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glShadeModel(GL_SMOOTH);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);

	glAlphaFunc( GL_GREATER, 0.25);
	glEnable( GL_ALPHA_TEST);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	const float NEARVAL = 0.1f;
	const float FARVAL = 10.0f;
	const float dim = NEARVAL * float(tan(M_PI / 8.0))*1.2;
	glFrustum(-dim, dim, -dim, dim, NEARVAL, FARVAL);

	SbMatrix mx;
	mx = cam->orientation.getValue();

	mx = mx.inverse();
	mx[3][2] = -5.0;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf((float*) mx);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	if (pickMode) {
		glDisable(GL_BLEND);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glShadeModel(GL_FLAT);
		glDisable(GL_DITHER);
		glDisable(GL_POLYGON_SMOOTH);
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	glClear(GL_DEPTH_BUFFER_BIT);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, (void*) m_VertexArray.data());
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, m_TextureCoordArray.data());

	if (!pickMode) {
		// Draw the axes
		glDisable(GL_TEXTURE_2D);
		float a=1.1;

		static GLubyte xbmp[] = { 0x11,0x11,0x0a,0x04,0x0a,0x11,0x11 };
		glColor3f(1, 0, 0);
		glBegin(GL_LINES);
		glVertex3f(-1 , -1, -1);
		glVertex3f(+1 , -1, -1);
		glEnd();
	    glRasterPos3d(a, -a, -a);
	    glBitmap(8, 7, 0, 0, 0, 0, xbmp);

		static GLubyte ybmp[] = { 0x04,0x04,0x04,0x04,0x0a,0x11,0x11 };
		glColor3f(0, 1, 0);
		glBegin(GL_LINES);
		glVertex3f(-1 , -1, -1);
		glVertex3f(-1 , +1, -1);
		glEnd();
	    glRasterPos3d( -a, a, -a);
	    glBitmap(8, 7, 0, 0, 0, 0, ybmp);

		static GLubyte zbmp[] = { 0x1f,0x10,0x08,0x04,0x02,0x01,0x1f };
		glColor3f(0, 0, 1);
		glBegin(GL_LINES);
		glVertex3f(-1 , -1, -1);
		glVertex3f(-1 , -1, +1);
		glEnd();
	    glRasterPos3d( -a, -a, a);
	    glBitmap(8, 7, 0, 0, 0, 0, zbmp);

		glEnable(GL_TEXTURE_2D);
	}

	// Draw the cube faces
	for (int pass = 0; pass < 2 ; pass++) {
		for (vector<Face*>::iterator f = m_Faces.begin(); f != m_Faces.end(); f++) {
			if (pickMode) {
				glColor3ub((*f)->m_PickId, 0, 0);
				glBindTexture(GL_TEXTURE_2D, (*f)->m_PickTextureId);
			} else {
				QColor& c = (m_HiliteId == (*f)->m_PickId) ? m_HiliteColor : (*f)->m_Color;
				glColor4f(c.redF(), c.greenF(), c.blueF(),c.alphaF());
				if (pass==0 && c.alphaF()<1.0)
					continue;
				if (pass==1 && c.alphaF()>=1.0)
					continue;
				glBindTexture(GL_TEXTURE_2D, (*f)->m_TextureId);
			}
			glDrawElements(GL_TRIANGLE_FAN, (*f)->m_VertexCount, GL_UNSIGNED_BYTE, (void*) &m_IndexArray[(*f)->m_FirstVertex]);
		}
	}




	// Draw the rotate buttons
	glEnable(GL_CULL_FACE);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDisable(GL_DEPTH_TEST);
	glClear(GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 1, 0, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	for (vector<int>::iterator b = m_Buttons.begin(); b != m_Buttons.end(); b++) {
		if (pickMode)
			glColor3ub(*b, 0, 0);
		else {
			QColor& c = (m_HiliteId ==(*b)) ? m_HiliteColor : m_ButtonColor;
			glColor3f(c.redF(), c.greenF(), c.blueF());
		}
		glBindTexture(GL_TEXTURE_2D, m_Textures[*b]);

		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex3f(0.0f, 1.0f, 0.0f);
		glTexCoord2f(1, 0);
		glVertex3f(1.0f, 1.0f, 0.0f);
		glTexCoord2f(1, 1);
		glVertex3f(1.0f, 0.0f, 0.0f);
		glTexCoord2f(0, 1);
		glVertex3f(0.0f, 0.0f, 0.0f);
		glEnd();
	}

	// Draw the view menu icon
	if (pickMode) {
		glColor3ub(TEX_VIEW_MENU, 0, 0);
		glBindTexture(GL_TEXTURE_2D, m_Textures[TEX_VIEW_MENU]);
	}
	else {
		glColor3ub(255,255,255);
		glBindTexture(GL_TEXTURE_2D, m_Textures[TEX_VIEW_MENU_ICON]);
	}

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3f(0.0f, 1.0f, 0.0f);
	glTexCoord2f(1, 0);
	glVertex3f(1.0f, 1.0f, 0.0f);
	glTexCoord2f(1, 1);
	glVertex3f(1.0f, 0.0f, 0.0f);
	glTexCoord2f(0, 1);
	glVertex3f(0.0f, 0.0f, 0.0f);
	glEnd();


	glPopMatrix();

	// Restore original state.

	glDepthRange(depthrange[0], depthrange[1]);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(projectionmatrix);

	glPopAttrib();
}

int NaviCubeImplementation::pickFace(SbVec2s pos) {
	GLubyte pixels[4] = {0};
	if (m_PickingFramebuffer) {
		m_PickingFramebuffer->bind();

		glViewport(0, 0, m_CubeWidgetSize, m_CubeWidgetSize);
		glLoadIdentity();

		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		drawNaviCube(true);

		glFinish();

		short x, y;
		pos.getValue(x, y);
		glReadPixels(x - (m_CubeWidgetPosX-m_CubeWidgetSize/2), y - (m_CubeWidgetPosY-m_CubeWidgetSize/2), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixels);
		m_PickingFramebuffer->release();

		QImage image = m_PickingFramebuffer->toImage();
		//image.save(QLatin1String("pickimage.png"));
	}
	return pixels[3] == 255 ? pixels[0] : 0;
}

bool NaviCubeImplementation::mousePressed(const SoMouseButtonEvent* mbev) {
	m_MouseDown = true;
	m_Dragging = false;
	m_MightDrag = inDragZone(mbev->getPosition());
	int pick=pickFace(mbev->getPosition());
	cerr << enum2str(pick) << endl;
	setHilite(pick);
	return pick != 0;
}

void NaviCubeImplementation::setView(float rotZ,float rotX) {
	SbRotation rz, rx, t;
	rz.setValue(SbVec3f(0, 0, 1), rotZ * M_PI / 180);
	rx.setValue(SbVec3f(1, 0, 0), rotX * M_PI / 180);
	m_View3DInventorViewer->setCameraOrientation(rx * rz);
}

void NaviCubeImplementation::rotateView(int axis,float rotAngle) {
	SbRotation viewRot = m_View3DInventorViewer->getCameraOrientation();

	SbVec3f up;
	viewRot.multVec(SbVec3f(0,1,0),up);

	SbVec3f out;
	viewRot.multVec(SbVec3f(0,0,1),out);

	SbVec3f& u = up;
	SbVec3f& o = out;
	SbVec3f right  (u[1]*o[2]-u[2]*o[1], u[2]*o[0]-u[0]*o[2], u[0]*o[1]-u[1]*o[0]);

	SbVec3f direction;
	switch (axis) {
	default :
		return;
	case DIR_UP :
		direction = up;
		break;
	case DIR_OUT :
		direction = out;
		break;
	case DIR_RIGHT :
		direction = right;
		break;
	}

	SbRotation rot(direction, -rotAngle*M_PI/180.0);
	SbRotation newViewRot = viewRot * rot;
	m_View3DInventorViewer->setCameraOrientation(newViewRot);

}

void NaviCubeImplementation::handleMenu() {
	QMenu menu;
	menu.addAction(new QAction(str("Test"),NULL));
	menu.addAction(new QAction(str("Test"),NULL));
	menu.addAction(new QAction(str("Test"),NULL));
	menu.addAction(new QAction(str("Test"),NULL));
	menu.addAction(new QAction(str("Test"),NULL));
	menu.addAction(new QAction(str("Test"),NULL));
	menu.addAction(new QAction(str("Test"),NULL));
	menu.addAction(new QAction(str("Test"),NULL));
	menu.exec(QCursor::pos());
}

bool NaviCubeImplementation::mouseReleased(const SoMouseButtonEvent* mbev) {
	setHilite(0);
	m_MouseDown = false;
	if (!m_Dragging) {
		float rot = 30;
		float tilt = 30;
		int pick = pickFace(mbev->getPosition());
		switch (pick) {
		default:
			return false;
			break;
		case TEX_FRONT:
			setView(0, 90);
			break;
		case TEX_REAR:
			setView(180, 90);
			break;
		case TEX_LEFT:
			setView(270,90);
			break;
		case TEX_RIGHT:
			setView(90,90);
			break;
		case TEX_TOP:
			setView(0,0);
			break;
		case TEX_BOTTOM:
			setView(0,180);
			break;
		case TEX_CORNER_BOTTOM_LEFT_FRONT:
			setView(rot - 90, 90 + tilt);
			break;
		case TEX_CORNER_BOTTOM_FRONT_RIGHT:
			setView(90 + rot - 90, 90 + tilt);
			break;
		case TEX_CORNER_BOTTOM_RIGHT_REAR:
			setView(180 + rot - 90, 90 + tilt);
			break;
		case TEX_CORNER_BOTTOM_REAR_LEFT:
			setView(270 + rot - 90, 90 + tilt);
			break;
		case TEX_CORNER_TOP_RIGHT_FRONT:
			setView(rot, 90 - tilt);
			break;
		case TEX_CORNER_TOP_FRONT_LEFT:
			setView(rot - 90, 90 - tilt);
			break;
		case TEX_CORNER_TOP_LEFT_REAR:
			setView(rot - 180, 90 - tilt);
			break;
		case TEX_CORNER_TOP_REAR_RIGHT:
			setView(rot - 270, 90 - tilt);
			break;
		case TEX_ARROW_LEFT :
			rotateView(DIR_OUT,45);
			break;
		case TEX_ARROW_RIGHT :
			rotateView(DIR_OUT,-45);
			break;
		case TEX_ARROW_WEST :
			rotateView(DIR_UP,-45);
			break;
		case TEX_ARROW_EAST :
			rotateView(DIR_UP,45);
			break;
		case TEX_ARROW_NORTH :
			rotateView(DIR_RIGHT,-45);
			break;
		case TEX_ARROW_SOUTH :
			rotateView(DIR_RIGHT,45);
			break;
		case TEX_VIEW_MENU :
			handleMenu();
			break;
		}
	}
	return true;
}


void NaviCubeImplementation::setHilite(int hilite) {
	if (hilite != m_HiliteId) {
		m_HiliteId = hilite;
		//cerr << "m_HiliteFace " << m_HiliteId << endl;
		m_View3DInventorViewer->getSoRenderManager()->scheduleRedraw();
	}
}

bool NaviCubeImplementation::inDragZone(const SbVec2s& pos) {
	short x,y;
	pos.getValue(x, y);
	int dx = x - m_CubeWidgetPosX;
	int dy = y - m_CubeWidgetPosY;
	int limit = m_CubeWidgetSize/4;
	return abs(dx)<limit && abs(dy)<limit;
}


bool NaviCubeImplementation::mouseMoved(const SoEvent* ev) {
	setHilite(pickFace(ev->getPosition()));

	if (m_MouseDown) {
		if (m_MightDrag && !m_Dragging && !inDragZone(ev->getPosition()))
			m_Dragging = true;
		if (m_Dragging) {
			setHilite(0);
			short x,y;
			ev->getPosition().getValue(x, y);
			m_CubeWidgetPosX = x;
			m_CubeWidgetPosY = y;
			this->m_View3DInventorViewer->getSoRenderManager()->scheduleRedraw();
			return true;
		}
	}
	return false;
}

bool NaviCubeImplementation::processSoEvent(const SoEvent* ev) {
	if (ev->getTypeId().isDerivedFrom(SoMouseButtonEvent::getClassTypeId())) {
		const SoMouseButtonEvent* mbev = static_cast<const SoMouseButtonEvent*>(ev);
		if (mbev->isButtonPressEvent(mbev, SoMouseButtonEvent::BUTTON1))
			return mousePressed(mbev);
		if (mbev->isButtonReleaseEvent(mbev, SoMouseButtonEvent::BUTTON1))
			return mouseReleased(mbev);
	}
	if (ev->getTypeId().isDerivedFrom(SoLocation2Event::getClassTypeId()))
		return mouseMoved(ev);
	return false;
}


QString NaviCubeImplementation::str(char* str) {
	return QString::fromLatin1(str);
}
