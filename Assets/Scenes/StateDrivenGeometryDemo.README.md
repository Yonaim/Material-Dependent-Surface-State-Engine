# State-Driven Geometry 데모

저장소 루트에서 `./run.ps1 -AppArgs @('--scene', 'Assets/Scenes/StateDrivenGeometryDemo.Scene')`로 실행하거나 Scene 창에서 **State-Driven Geometry**를 선택한다. 씬을 열면 11초 클립이 자동 재생된다. **Animation → Replay demo** 또는 **F9**는 Scene과 Surface State를 함께 초기화하고 다시 재생한다. **F10**은 촬영용 전체 뷰와 편집 UI를 전환하고, Capture View 상태에서는 **Esc**로 편집 UI로 돌아온다.

`../Animations/StateDrivenGeometryDemo.DemoAnim`은 중앙 머드를 다섯 번에 걸쳐 적층하고, 원통형 막대를 내릴 때 머드 일부를 제거한다. 막대가 비켜난 뒤 노즐이 움푹한 곳 안쪽으로 이동해 waterfilm을 네 번 추가한다. 마지막에는 카메라가 가까워져 결과를 보여준다.

바닥은 넓고 얇게 확대한 MarbleCube다. 접촉 위치는 바닥의 로컬 좌표로 기록하며 `depth`로 윗면만 선택한다. 막대와 노즐은 화면에 보이는 메시이고, State 변경은 클립의 시간 지정 접촉 이벤트가 수행한다.

머드 적층이 잘 보이도록 데모 프로필은 `accumulationFactor: 1.0`, `thicknessPerAmount: 0.45`를 사용한다. 프로필 기준 최대 적층 높이는 약 0.4 world unit이다.

접촉 이벤트가 있는 클립은 화면 프레임마다 일정한 시뮬레이션 시간만 진행한다. 수동 입력이 필요할 때는 Contact Input 창에서 Add/Remove와 Circle/Ellipse/Rectangle/Capsule을 선택할 수 있다.
