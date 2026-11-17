#pragma once

#include "Mesh.h"

//--- 선수 인형 모델(player_red/blue.obj) 공통 치수
// 받침(높이 0.14) 위 몸 높이가 2.065 단위라, 실제 키 1.85m에 맞춰 줄인다.
constexpr float PLAYER_HEIGHT_M = 1.85f;
constexpr float PLAYER_SCALE = meters(PLAYER_HEIGHT_M) / 2.065f;

// 모델 좌표의 손 위치 (줄이기 전). 모델은 +Z를 바라보므로 오른손이 -X 쪽이다.
const glm::vec3 PLAYER_RIGHT_HAND(-0.582f, 0.66f, 0.0f);
// 모델 좌표의 머리 반지름 (줄이기 전)
constexpr float PLAYER_HEAD_RADIUS = 0.32f;
