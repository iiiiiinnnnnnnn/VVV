// GameTime.h
#pragma once

namespace Game
{
	class Time
	{
	public:
		static float time; // �N������̗݌v����
		static float scale; // ���Ԃ̃X�P�[��
		static float deltaTime; // �O�t���[������̌o�ߎ���
		static float unscaledDeltaTime; // �N������̗݌v���ԁi�X�P�[���̉e����󂯂Ȃ��j
	};
}
