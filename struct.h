float X, width;
float Y, height;
namespace GetStructs {
	typedef struct {
		float Pitch;
		float Yaw;
		float Roll;
	} FRotator;

	typedef struct {
		float X, Y, Z;
	} FVector;

	typedef struct {
		FVector Location;
		FRotator Rotation;
		float FOV;
		float OrthoWidth;
		float OrthoNearClipPlane;
		float OrthoFarClipPlane;
		float AspectRatio;
	} FMinimalViewInfo;
}

struct FBox
{
	Vector3  Min;
	Vector3  Max;
	unsigned char IsValid;
	unsigned char UnknownData00[0x3];
};

struct FVector {
	float X, Y, Z;

	inline bool IsZero()
	{
		if (X == 0 && Y == 0 && Z == 0)
			return true;

		return false;
	}
};

typedef struct {
	Vector3 Location;
	Vector3 Rotation;
	float FOV;
	float OrthoWidth;
	float OrthoNearClipPlane;
	float OrthoFarClipPlane;
	float AspectRatio;
} FMinimalViewInfo;

struct FMatrix
{
	float M[4][4];
};

typedef struct {
	FVector ViewOrigin;
	char _padding_0[4];
	FMatrix ViewRotationMatrix;
	FMatrix ProjectionMatrix;
} FSceneViewProjectionData;

struct {
	FMinimalViewInfo Info;
	float ProjectionMatrix[4][4];
} view;