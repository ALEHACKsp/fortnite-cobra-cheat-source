namespace Settings
{
	bool ShowMenu = true;
	bool PlayerESP = true;
	bool CornerESP = false;
	bool ThreeDESP = false;
	bool Filled = false;
	bool OutlinedESP = false;
	bool Distance = false;
	bool Radar = true;
	float RadarDistance = 50000.f;
	bool MouseAimbot = true;
	bool Skeleton = true;
	bool AimbotCircle = true;
	float AimbotFOV = 300.f;
	float Roughness = 70.f;
	bool Crosshair = true;
	bool Reticle = false;
	float Smoothing = 3;
	bool Lines = true;
	bool isaimbotting;
	bool Selfesp = false;
	bool RandomAim = true;
	bool aimClosest = false;
	bool OutlinedSkeleton = true;
	bool isFortniteFocus;
	int bones;
	bool Memory = true;
	bool VisibleCheck = true;
	struct col
	{
		int r, g, b, a;
	};
	col BoxESP;
	//col Lines;
	static int hitbox;
	static int hitboxpos = 0;
	float BoxCol = ImColor(255, 255, 255, 255);
	float LineCol = ImColor(255, 255, 255, 255);
	DWORD_PTR entityx;

	int MaxFov = 0;
}