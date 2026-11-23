#pragma once

class GamePadVib
{
  public:
	static GamePadVib& Instance()
	{
		static GamePadVib instance;
		return instance;
	}
	~GamePadVib();
	void Initialize();
	void Vibrate(float leftMotorPower, float rightMotorPower, float duration);
	Vector2 GetVibration();
	void Update();

  private:
	float duration = 0.0f;
	float leftPower = 0.0f;
	float rightPower = 0.0f;
};