#ifndef PROFILE_ENGINE_H
#define PROFILE_ENGINE_H

#include "SharedData.h"

class ProfileEngine {
public:
    ProfileEngine();

    void begin();
    int calculateTargetTemp(int elapsedSeconds, const ReflowRecipe &recipe);
    float getTargetRampRate(int elapsedSeconds, const ReflowRecipe &recipe) const;
    ProfileStage getCurrentStage(int elapsedSeconds, const ReflowRecipe &recipe);
    int getTotalDuration(const ReflowRecipe &recipe) const;

private:
    int getPreheatEnd(const ReflowRecipe &recipe) const;
    int getSoakEnd(const ReflowRecipe &recipe) const;
    int getReflowPeakEnd(const ReflowRecipe &recipe) const;
    int getReflowEnd(const ReflowRecipe &recipe) const;
};

#endif
