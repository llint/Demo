//----------------------------------------------------------------------------//
// demo.h                                                                     //
// Copyright (C) 2001 Bruno 'Beosil' Heidelberger                             //
//----------------------------------------------------------------------------//
// This program is free software; you can redistribute it and/or modify it    //
// under the terms of the GNU General Public License as published by the Free //
// Software Foundation; either version 2 of the License, or (at your option)  //
// any later version.                                                         //
//----------------------------------------------------------------------------//

#ifndef DEMO_H
#define DEMO_H

//----------------------------------------------------------------------------//
// Includes                                                                   //
//----------------------------------------------------------------------------//

#include "global.h"

//----------------------------------------------------------------------------//
// Forward declarations                                                       //
//----------------------------------------------------------------------------//

class Model;
typedef std::unique_ptr<Model> ModelPtr;

//----------------------------------------------------------------------------//
// Class declaration                                                          //
//----------------------------------------------------------------------------//

#include "GameEngineSystem.h"

// Integration thoughts:
// Let this Demo class remain as close as originally, and it handles mostly just opengl rendering stuff and models
// our engine class never knows about models (it's created and managed inside Demo, which also provides user input management, and feed the updated state to Entity
// Demo can simply inherit from Engine, and implement the two OnEntityCreated/Deleted callbacks, or the Demo could become a listener of Engine, so they are less coupled
// for simplicity's sake, we prefer the inheritance approach

// Then Engine will just become a reference of ServerEngine or ClientEngine.

class Demo : public Engine
{
// member variables
protected:
  int m_width;
  int m_height;
  bool m_bFullscreen;
  float m_fpsDuration;
  int m_fpsFrames;
  int m_fps;
  GLuint m_cursorTextureId;
  GLuint m_logoTextureId;
  GLuint m_fpsTextureId;
  int m_mouseX;
  int m_mouseY;
  float m_tiltAngle;
  float m_twistAngle;
  float m_distance;
  bool m_bLeftMouseButtonDown;
  bool m_bRightMouseButtonDown;
  unsigned int m_lastTick;
  std::string m_strDatapath;
  std::string m_strCal3D_Datapath;
  //std::vector<Model *> m_vectorModel;
  //unsigned int m_currentModel;
  bool m_bPaused;
	float m_averageCPUTime;
	bool m_bOutputAverageCPUTimeAtExit;

    std::unordered_map<Entity::weak_ptr, ModelPtr> m_models;

protected:
    ModelPtr CreateModel();

    void OnEntityCreated(Entity::weak_ptr entity) override;
    void OnEntityDeleted(Entity::weak_ptr entity) override;

// constructors/destructor
public:
  Demo();
  virtual ~Demo();

// member functions
public:
  std::string getCaption();
  std::string getDatapath();
  bool getFullscreen();
  int getHeight();
  Model *getModel();
  int getWidth();
  bool loadTexture(const std::string& strFllename, GLuint& pId);
  void nextModel();
  bool onCreate(int argc, char *argv[]);
  void onIdle();
  bool onInit();
  void onKey(unsigned char key, int x, int y);
  void onMouseButtonDown(int button, int x, int y);
  void onMouseButtonUp(int button, int x, int y);
  void onMouseMove(int x, int y);
  void onRender();
  void onShutdown();
  void setDimension(int width, int height);
};

extern Demo theDemo;

#endif

//----------------------------------------------------------------------------//
