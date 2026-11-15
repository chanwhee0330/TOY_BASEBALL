# TOY BASEBALL

Visual Studio에서 OpenGL 3.3으로 야구장 모델을 띄우는 프로젝트입니다.

1. **C++를 사용한 데스크톱 개발** 워크로드와 Windows SDK가 설치된 Visual Studio에서 `TOY_BASEBALL.sln`을 엽니다.
2. **Debug / x64**를 선택하고 **Ctrl+F5**로 실행합니다.
3. 사선 위에서 내려다본 경기장이 표시됩니다. **Q** 또는 **Esc**로 닫을 수 있습니다.

GLEW, GLFW, GLM의 x64 빌드에 필요한 파일은 `Dependencies`에 있습니다. 빌드하면 셰이더, DLL, 경기장 OBJ/MTL이 실행 파일이 있는 `bin/x64/Debug`에 복사됩니다. 의존성 설정은 `GraphicsCommon.props`에 있습니다.

기존 Project2의 흐름에 맞춰 `Game/TOY_BASEBALL.cpp`의 `main()`에서 창을 준비하고, `InitBuffer()`에서 모델을 올리고, `drawScene()`에서 카메라와 조명을 적용합니다. `Game/Model.cpp`가 `Assets/Models/stadium.obj`의 삼각형과 법선, MTL의 재질색과 발광색을 읽습니다.
