// Copyright Xyzto Works

#include "Misc/AutomationTest.h"
#include "Types/ToolRingRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneToolRingPickSliceTest, "Nevergone.Excursion.ToolRing.PickSlice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneToolRingPickSliceTest::RunTest(const FString& Parameters)
{
	// Two slices: the top one and the bottom one.
	TestEqual(TEXT("Up picks slice 0"), ToolRingRules::PickSlice(FVector2D(0.0f, 100.0f), 2, 10.0f), 0);
	TestEqual(TEXT("Down picks slice 1"), ToolRingRules::PickSlice(FVector2D(0.0f, -100.0f), 2, 10.0f), 1);

	// Four slices go clockwise from the top.
	TestEqual(TEXT("Up"), ToolRingRules::PickSlice(FVector2D(0.0f, 100.0f), 4, 10.0f), 0);
	TestEqual(TEXT("Right"), ToolRingRules::PickSlice(FVector2D(100.0f, 0.0f), 4, 10.0f), 1);
	TestEqual(TEXT("Down"), ToolRingRules::PickSlice(FVector2D(0.0f, -100.0f), 4, 10.0f), 2);
	TestEqual(TEXT("Left"), ToolRingRules::PickSlice(FVector2D(-100.0f, 0.0f), 4, 10.0f), 3);

	// Slightly left of up still belongs to the top slice, which straddles zero.
	TestEqual(TEXT("Up and a little left"), ToolRingRules::PickSlice(FVector2D(-10.0f, 100.0f), 4, 10.0f), 0);

	// One slice takes every direction.
	TestEqual(TEXT("A single slice"), ToolRingRules::PickSlice(FVector2D(-70.0f, -30.0f), 1, 10.0f), 0);

	// Dead zone and no slices keep the highlight where it was.
	TestEqual(TEXT("Dead zone"), ToolRingRules::PickSlice(FVector2D(0.0f, 5.0f), 4, 10.0f), (int32)INDEX_NONE);
	TestEqual(TEXT("Centre"), ToolRingRules::PickSlice(FVector2D::ZeroVector, 4, 0.0f), (int32)INDEX_NONE);
	TestEqual(TEXT("No slices"), ToolRingRules::PickSlice(FVector2D(0.0f, 100.0f), 0, 10.0f), (int32)INDEX_NONE);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneToolRingPointerTest, "Nevergone.Excursion.ToolRing.Pointer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneToolRingPointerTest::RunTest(const FString& Parameters)
{
	const FVector2D Inside = ToolRingRules::AdvancePointer(FVector2D(10.0f, 0.0f), FVector2D(20.0f, 0.0f), 100.0f);

	TestEqual(TEXT("Inside the radius it just moves"), Inside.X, 30.0);

	const FVector2D Clamped = ToolRingRules::AdvancePointer(FVector2D(90.0f, 0.0f), FVector2D(60.0f, 0.0f), 100.0f);

	TestEqual(TEXT("Past the radius it stops at the edge"), Clamped.Size(), 100.0);
	TestEqual(TEXT("and keeps its direction"), Clamped.X, 100.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNevergoneToolRingSelectionTest, "Nevergone.Excursion.ToolRing.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNevergoneToolRingSelectionTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Both = { FName("Bait"), FName("Distraction") };

	TestEqual(TEXT("The selected item stays while available"),
		ToolRingRules::ResolveSelection(Both, FName("Distraction")), 1);

	const TArray<FName> OnlyDistraction = { FName("Distraction") };

	TestEqual(TEXT("A used-up item passes the selection to the next available"),
		ToolRingRules::ResolveSelection(OnlyDistraction, FName("Bait")), 0);

	TestEqual(TEXT("Nothing selected yet picks the first"),
		ToolRingRules::ResolveSelection(Both, NAME_None), 0);

	TestEqual(TEXT("Nothing available selects nothing"),
		ToolRingRules::ResolveSelection(TArray<FName>(), FName("Bait")), (int32)INDEX_NONE);

	return true;
}

#endif
