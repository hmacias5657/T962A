#include "ProfileEngine.h"
#include "Config.h"

ProfileEngine::ProfileEngine() {}

void ProfileEngine::begin() {}

int ProfileEngine::getPreheatEnd(const ReflowRecipe &recipe) const {
    return recipe.preheatRampTime;
}

int ProfileEngine::getSoakEnd(const ReflowRecipe &recipe) const {
    return getPreheatEnd(recipe) + recipe.soakTime;
}

int ProfileEngine::getReflowPeakEnd(const ReflowRecipe &recipe) const {
    return getSoakEnd(recipe) + recipe.reflowTime;
}

int ProfileEngine::getReflowEnd(const ReflowRecipe &recipe) const {
    return getReflowPeakEnd(recipe) + recipe.peakHoldTime;
}

int ProfileEngine::getTotalDuration(const ReflowRecipe &recipe) const {
    if (recipe.type == RECIPE_BAKE) {
        return recipe.bakeDuration * 60;
    }
    return getPreheatEnd(recipe) + recipe.soakTime + recipe.reflowTime + recipe.peakHoldTime + 60;
}

int ProfileEngine::calculateTargetTemp(int elapsedSeconds, const ReflowRecipe &recipe) {
    if (recipe.type == RECIPE_BAKE) {
        return recipe.bakeTemp;
    }

    int target = AMBIENT_TEMP;
    int tPreheatEnd = getPreheatEnd(recipe);
    int tSoakEnd = getSoakEnd(recipe);
    int tReflowPeak = getReflowPeakEnd(recipe);
    int tReflowEnd = getReflowEnd(recipe);

    if (elapsedSeconds <= tPreheatEnd) {
        target = AMBIENT_TEMP + ((recipe.preheatTemp - AMBIENT_TEMP) * elapsedSeconds) / tPreheatEnd;
    } else if (elapsedSeconds <= tSoakEnd) {
        int deltaT = elapsedSeconds - tPreheatEnd;
        target = recipe.preheatTemp + ((recipe.soakTemp - recipe.preheatTemp) * deltaT) / recipe.soakTime;
    } else if (elapsedSeconds <= tReflowPeak) {
        int deltaT = elapsedSeconds - tSoakEnd;
        target = recipe.soakTemp + ((recipe.peakTemp - recipe.soakTemp) * deltaT) / recipe.reflowTime;
    } else if (elapsedSeconds <= tReflowEnd) {
        int deltaT = elapsedSeconds - tReflowPeak;
        target = recipe.peakTemp - ((recipe.peakTemp - 150) * deltaT) / recipe.peakHoldTime;
    } else {
        int deltaT = elapsedSeconds - tReflowEnd;
        target = 150 - (2 * deltaT);
        if (target < 40) target = 40;
    }

    return target;
}

ProfileStage ProfileEngine::getCurrentStage(int elapsedSeconds, const ReflowRecipe &recipe) {
    if (recipe.type == RECIPE_BAKE) {
        return STAGE_SOAK;
    }

    int tPreheatEnd = getPreheatEnd(recipe);
    int tSoakEnd = getSoakEnd(recipe);
    int tReflowPeak = getReflowPeakEnd(recipe);
    int tReflowEnd = getReflowEnd(recipe);

    if (elapsedSeconds <= tPreheatEnd) return STAGE_PREHEAT;
    if (elapsedSeconds <= tSoakEnd) return STAGE_SOAK;
    if (elapsedSeconds <= tReflowPeak) return STAGE_REFLOW_RAMP;
    if (elapsedSeconds <= tReflowEnd) return STAGE_REFLOW_PEAK;
    return STAGE_COOLDOWN;
}
