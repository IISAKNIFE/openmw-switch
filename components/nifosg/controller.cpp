#include "controller.hpp"

#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/TexMat>
#include <osg/Texture2D>

#include <osgParticle/Emitter>

#include <components/nif/data.hpp>
#include <components/sceneutil/morphgeometry.hpp>

#include "matrixtransform.hpp"

namespace NifOsg
{

    ControllerFunction::ControllerFunction(const Nif::Controller* ctrl)
        : mFrequency(ctrl->frequency)
        , mPhase(ctrl->phase)
        , mStartTime(ctrl->timeStart)
        , mStopTime(ctrl->timeStop)
        , mExtrapolationMode(ctrl->extrapolationMode())
    {
    }

    float ControllerFunction::calculate(float value) const
    {
        float time = mFrequency * value + mPhase;
        if (time >= mStartTime && time <= mStopTime)
            return time;
        switch (mExtrapolationMode)
        {
            case Nif::Controller::ExtrapolationMode::Cycle:
            {
                float delta = mStopTime - mStartTime;
                if (delta <= 0)
                    return mStartTime;
                float cycles = (time - mStartTime) / delta;
                float remainder = (cycles - std::floor(cycles)) * delta;
                return mStartTime + remainder;
            }
            case Nif::Controller::ExtrapolationMode::Reverse:
            {
                float delta = mStopTime - mStartTime;
                if (delta <= 0)
                    return mStartTime;

                float cycles = (time - mStartTime) / delta;
                float remainder = (cycles - std::floor(cycles)) * delta;

                // Even number of cycles?
                if ((static_cast<int>(std::fabs(std::floor(cycles))) % 2) == 0)
                    return mStartTime + remainder;

                return mStopTime - remainder;
            }
            case Nif::Controller::ExtrapolationMode::Constant:
            default:
                return std::clamp(time, mStartTime, mStopTime);
        }
    }

    float ControllerFunction::getMaximum() const
    {
        return mStopTime;
    }

    KeyframeController::KeyframeController() {}

    KeyframeController::KeyframeController(const KeyframeController& copy, const osg::CopyOp& copyop)
        : osg::Object(copy, copyop)
        , SceneUtil::KeyframeController(copy)
        , SceneUtil::NodeCallback<KeyframeController, NifOsg::MatrixTransform*>(copy, copyop)
        , mRotations(copy.mRotations)
        , mXRotations(copy.mXRotations)
        , mYRotations(copy.mYRotations)
        , mZRotations(copy.mZRotations)
        , mTranslations(copy.mTranslations)
        , mScales(copy.mScales)
        , mAxisOrder(copy.mAxisOrder)
    {
    }

    KeyframeController::KeyframeController(const Nif::NiKeyframeController* keyctrl)
    {
        if (!keyctrl->mInterpolator.empty())
        {
            if (keyctrl->mInterpolator->recType == Nif::RC_NiTransformInterpolator)
            {
                const Nif::NiTransformInterpolator* interp
                    = static_cast<const Nif::NiTransformInterpolator*>(keyctrl->mInterpolator.getPtr());
                if (!interp->data.empty())
                {
                    mRotations = QuaternionInterpolator(interp->data->mRotations, interp->defaultRot);
                    mXRotations = FloatInterpolator(interp->data->mXRotations);
                    mYRotations = FloatInterpolator(interp->data->mYRotations);
                    mZRotations = FloatInterpolator(interp->data->mZRotations);
                    mTranslations = Vec3Interpolator(interp->data->mTranslations, interp->defaultPos);
                    mScales = FloatInterpolator(interp->data->mScales, interp->defaultScale);
                    mAxisOrder = interp->data->mAxisOrder;
                }
                else
                {
                    mRotations = QuaternionInterpolator(Nif::QuaternionKeyMapPtr(), interp->defaultRot);
                    mTranslations = Vec3Interpolator(Nif::Vector3KeyMapPtr(), interp->defaultPos);
                    mScales = FloatInterpolator(Nif::FloatKeyMapPtr(), interp->defaultScale);
                }
            }
        }
        else if (!keyctrl->mData.empty())
        {
            const Nif::NiKeyframeData* keydata = keyctrl->mData.getPtr();
            mRotations = QuaternionInterpolator(keydata->mRotations);
            mXRotations = FloatInterpolator(keydata->mXRotations);
            mYRotations = FloatInterpolator(keydata->mYRotations);
            mZRotations = FloatInterpolator(keydata->mZRotations);
            mTranslations = Vec3Interpolator(keydata->mTranslations);
            mScales = FloatInterpolator(keydata->mScales, 1.f);
            mAxisOrder = keydata->mAxisOrder;
        }
    }

    osg::Quat KeyframeController::getXYZRotation(float time) const
    {
        float xrot = 0, yrot = 0, zrot = 0;
        if (!mXRotations.empty())
            xrot = mXRotations.interpKey(time);
        if (!mYRotations.empty())
            yrot = mYRotations.interpKey(time);
        if (!mZRotations.empty())
            zrot = mZRotations.interpKey(time);
        osg::Quat xr(xrot, osg::X_AXIS);
        osg::Quat yr(yrot, osg::Y_AXIS);
        osg::Quat zr(zrot, osg::Z_AXIS);
        switch (mAxisOrder)
        {
            case Nif::NiKeyframeData::AxisOrder::Order_XYZ:
                return xr * yr * zr;
            case Nif::NiKeyframeData::AxisOrder::Order_XZY:
                return xr * zr * yr;
            case Nif::NiKeyframeData::AxisOrder::Order_YZX:
                return yr * zr * xr;
            case Nif::NiKeyframeData::AxisOrder::Order_YXZ:
                return yr * xr * zr;
            case Nif::NiKeyframeData::AxisOrder::Order_ZXY:
                return zr * xr * yr;
            case Nif::NiKeyframeData::AxisOrder::Order_ZYX:
                return zr * yr * xr;
            case Nif::NiKeyframeData::AxisOrder::Order_XYX:
                return xr * yr * xr;
            case Nif::NiKeyframeData::AxisOrder::Order_YZY:
                return yr * zr * yr;
            case Nif::NiKeyframeData::AxisOrder::Order_ZXZ:
                return zr * xr * zr;
        }
        return xr * yr * zr;
    }

    osg::Vec3f KeyframeController::getTranslation(float time) const
    {
        if (!mTranslations.empty())
            return mTranslations.interpKey(time);
        return osg::Vec3f();
    }

    void KeyframeController::operator()(NifOsg::MatrixTransform* node, osg::NodeVisitor* nv)
    {
        if (hasInput())
        {
            float time = getInputValue(nv);

            if (!mRotations.empty())
                node->setRotation(mRotations.interpKey(time));
            else if (!mXRotations.empty() || !mYRotations.empty() || !mZRotations.empty())
                node->setRotation(getXYZRotation(time));
            else
                node->setRotation(node->mRotationScale);

            if (!mScales.empty())
                node->setScale(mScales.interpKey(time));

            if (!mTranslations.empty())
                node->setTranslation(mTranslations.interpKey(time));
        }

        traverse(node, nv);
    }

    GeomMorpherController::GeomMorpherController() {}

    GeomMorpherController::GeomMorpherController(const GeomMorpherController& copy, const osg::CopyOp& copyop)
        : Controller(copy)
        , SceneUtil::NodeCallback<GeomMorpherController, SceneUtil::MorphGeometry*>(copy, copyop)
        , mKeyFrames(copy.mKeyFrames)
        , mWeights(copy.mWeights)
    {
    }

    GeomMorpherController::GeomMorpherController(const Nif::NiGeomMorpherController* ctrl)
    {
        if (ctrl->mInterpolators.size() == 0)
        {
            if (!ctrl->mData.empty())
            {
                for (const auto& morph : ctrl->mData->mMorphs)
                    mKeyFrames.emplace_back(morph.mKeyFrames);
            }
            return;
        }

        mKeyFrames.resize(ctrl->mInterpolators.size());
        mWeights = ctrl->mWeights;

        for (std::size_t i = 0, n = ctrl->mInterpolators.size(); i < n; ++i)
        {
            if (!ctrl->mInterpolators[i].empty() && ctrl->mInterpolators[i]->recType == Nif::RC_NiFloatInterpolator)
            {
                auto interpolator = static_cast<const Nif::NiFloatInterpolator*>(ctrl->mInterpolators[i].getPtr());
                mKeyFrames[i] = FloatInterpolator(interpolator);
            }
        }
    }

    void GeomMorpherController::operator()(SceneUtil::MorphGeometry* node, osg::NodeVisitor* nv)
    {
        if (hasInput())
        {
            if (mKeyFrames.size() <= 1)
                return;
            float input = getInputValue(nv);
            size_t i = 1;
            for (std::vector<FloatInterpolator>::iterator it = mKeyFrames.begin() + 1; it != mKeyFrames.end();
                 ++it, ++i)
            {
                float val = 0;
                if (!(*it).empty())
                {
                    val = it->interpKey(input);
                    if (i < mWeights.size())
                        val *= mWeights[i];
                }

                SceneUtil::MorphGeometry::MorphTarget& target = node->getMorphTarget(i);
                if (target.getWeight() != val)
                {
                    target.setWeight(val);
                    node->dirty();
                }
            }
        }
    }

    UVController::UVController() {}

    UVController::UVController(const Nif::NiUVData* data, const std::set<int>& textureUnits)
        : mUTrans(data->mKeyList[0], 0.f)
        , mVTrans(data->mKeyList[1], 0.f)
        , mUScale(data->mKeyList[2], 1.f)
        , mVScale(data->mKeyList[3], 1.f)
        , mTextureUnits(textureUnits)
    {
    }

    UVController::UVController(const UVController& copy, const osg::CopyOp& copyop)
        : osg::Object(copy, copyop)
        , StateSetUpdater(copy, copyop)
        , Controller(copy)
        , mUTrans(copy.mUTrans)
        , mVTrans(copy.mVTrans)
        , mUScale(copy.mUScale)
        , mVScale(copy.mVScale)
        , mTextureUnits(copy.mTextureUnits)
    {
    }

    void UVController::setDefaults(osg::StateSet* stateset)
    {
        osg::ref_ptr<osg::TexMat> texMat(new osg::TexMat);
        for (std::set<int>::const_iterator it = mTextureUnits.begin(); it != mTextureUnits.end(); ++it)
            stateset->setTextureAttributeAndModes(*it, texMat, osg::StateAttribute::ON);
    }

    void UVController::apply(osg::StateSet* stateset, osg::NodeVisitor* nv)
    {
        if (hasInput())
        {
            float value = getInputValue(nv);

            // First scale the UV relative to its center, then apply the offset.
            // U offset is flipped regardless of the graphics library,
            // while V offset is flipped to account for OpenGL Y axis convention.
            osg::Vec3f uvOrigin(0.5f, 0.5f, 0.f);
            osg::Vec3f uvScale(mUScale.interpKey(value), mVScale.interpKey(value), 1.f);
            osg::Vec3f uvTrans(-mUTrans.interpKey(value), -mVTrans.interpKey(value), 0.f);

            osg::Matrixf mat = osg::Matrixf::translate(uvOrigin);
            mat.preMultScale(uvScale);
            mat.preMultTranslate(-uvOrigin);
            mat.setTrans(mat.getTrans() + uvTrans);

            // setting once is enough because all other texture units share the same TexMat (see setDefaults).
            if (!mTextureUnits.empty())
            {
                osg::TexMat* texMat = static_cast<osg::TexMat*>(
                    stateset->getTextureAttribute(*mTextureUnits.begin(), osg::StateAttribute::TEXMAT));
                texMat->setMatrix(mat);
            }
        }
    }

    VisController::VisController(const Nif::NiVisController* ctrl, unsigned int mask)
        : mMask(mask)
    {
        if (!ctrl->mInterpolator.empty())
        {
            if (ctrl->mInterpolator->recType == Nif::RC_NiBoolInterpolator)
                mInterpolator
                    = ByteInterpolator(static_cast<const Nif::NiBoolInterpolator*>(ctrl->mInterpolator.getPtr()));
        }
        else if (!ctrl->mData.empty())
            mData = ctrl->mData->mVis;
    }

    VisController::VisController() {}

    VisController::VisController(const VisController& copy, const osg::CopyOp& copyop)
        : SceneUtil::NodeCallback<VisController>(copy, copyop)
        , Controller(copy)
        , mData(copy.mData)
        , mInterpolator(copy.mInterpolator)
        , mMask(copy.mMask)
    {
    }

    bool VisController::calculate(float time) const
    {
        if (!mInterpolator.empty())
            return mInterpolator.interpKey(time);

        if (mData.size() == 0)
            return true;

        for (size_t i = 1; i < mData.size(); i++)
        {
            if (mData[i].time > time)
                return mData[i - 1].isSet;
        }
        return mData.back().isSet;
    }

    void VisController::operator()(osg::Node* node, osg::NodeVisitor* nv)
    {
        if (hasInput())
        {
            bool vis = calculate(getInputValue(nv));
            node->setNodeMask(vis ? ~0 : mMask);
        }
        traverse(node, nv);
    }

    RollController::RollController(const Nif::NiRollController* ctrl)
    {
        if (!ctrl->mInterpolator.empty())
        {
            if (ctrl->mInterpolator->recType == Nif::RC_NiFloatInterpolator)
                mData = FloatInterpolator(static_cast<const Nif::NiFloatInterpolator*>(ctrl->mInterpolator.getPtr()));
        }
        else if (!ctrl->mData.empty())
            mData = FloatInterpolator(ctrl->mData->mKeyList, 1.f);
    }

    RollController::RollController(const RollController& copy, const osg::CopyOp& copyop)
        : SceneUtil::NodeCallback<RollController, osg::MatrixTransform*>(copy, copyop)
        , Controller(copy)
        , mData(copy.mData)
        , mStartingTime(copy.mStartingTime)
    {
    }

    void RollController::operator()(osg::MatrixTransform* node, osg::NodeVisitor* nv)
    {
        traverse(node, nv);

        if (hasInput())
        {
            double newTime = nv->getFrameStamp()->getSimulationTime();
            double duration = newTime - mStartingTime;
            mStartingTime = newTime;

            float value = mData.interpKey(getInputValue(nv));

            // Rotate around "roll" axis.
            // Note: in original game rotation speed is the framerate-dependent in a very tricky way.
            // Do not replicate this behaviour until we will really need it.
            // For now consider controller's current value as an angular speed in radians per 1/60 seconds.
            node->preMult(osg::Matrix::rotate(value * duration * 60.f, 0, 0, 1));

            // Note: doing it like this means RollControllers are not compatible with KeyframeControllers.
            // KeyframeController currently wins the conflict.
            // However unlikely that is, NetImmerse might combine the transformations somehow.
        }
    }

    AlphaController::AlphaController() {}

    AlphaController::AlphaController(const Nif::NiAlphaController* ctrl, const osg::Material* baseMaterial)
        : mBaseMaterial(baseMaterial)
    {
        if (!ctrl->mInterpolator.empty())
        {
            if (ctrl->mInterpolator->recType == Nif::RC_NiFloatInterpolator)
                mData = FloatInterpolator(static_cast<const Nif::NiFloatInterpolator*>(ctrl->mInterpolator.getPtr()));
        }
        else if (!ctrl->mData.empty())
            mData = FloatInterpolator(ctrl->mData->mKeyList, 1.f);
    }

    AlphaController::AlphaController(const AlphaController& copy, const osg::CopyOp& copyop)
        : StateSetUpdater(copy, copyop)
        , Controller(copy)
        , mData(copy.mData)
        , mBaseMaterial(copy.mBaseMaterial)
    {
    }

    void AlphaController::setDefaults(osg::StateSet* stateset)
    {
        stateset->setAttribute(
            static_cast<osg::Material*>(mBaseMaterial->clone(osg::CopyOp::DEEP_COPY_ALL)), osg::StateAttribute::ON);
    }

    void AlphaController::apply(osg::StateSet* stateset, osg::NodeVisitor* nv)
    {
        if (hasInput())
        {
            float value = mData.interpKey(getInputValue(nv));
            osg::Material* mat = static_cast<osg::Material*>(stateset->getAttribute(osg::StateAttribute::MATERIAL));
            osg::Vec4f diffuse = mat->getDiffuse(osg::Material::FRONT_AND_BACK);
            diffuse.a() = value;
            mat->setDiffuse(osg::Material::FRONT_AND_BACK, diffuse);
        }
    }

    MaterialColorController::MaterialColorController() {}

    MaterialColorController::MaterialColorController(
        const Nif::NiMaterialColorController* ctrl, const osg::Material* baseMaterial)
        : mTargetColor(static_cast<MaterialColorController::TargetColor>(ctrl->mTargetColor))
        , mBaseMaterial(baseMaterial)
    {
        if (!ctrl->mInterpolator.empty())
        {
            if (ctrl->mInterpolator->recType == Nif::RC_NiPoint3Interpolator)
                mData = Vec3Interpolator(static_cast<const Nif::NiPoint3Interpolator*>(ctrl->mInterpolator.getPtr()));
        }
        else if (!ctrl->mData.empty())
            mData = Vec3Interpolator(ctrl->mData->mKeyList, osg::Vec3f(1, 1, 1));
    }

    MaterialColorController::MaterialColorController(const MaterialColorController& copy, const osg::CopyOp& copyop)
        : StateSetUpdater(copy, copyop)
        , Controller(copy)
        , mData(copy.mData)
        , mTargetColor(copy.mTargetColor)
        , mBaseMaterial(copy.mBaseMaterial)
    {
    }

    void MaterialColorController::setDefaults(osg::StateSet* stateset)
    {
        stateset->setAttribute(
            static_cast<osg::Material*>(mBaseMaterial->clone(osg::CopyOp::DEEP_COPY_ALL)), osg::StateAttribute::ON);
    }

    void MaterialColorController::apply(osg::StateSet* stateset, osg::NodeVisitor* nv)
    {
        if (hasInput())
        {
            osg::Vec3f value = mData.interpKey(getInputValue(nv));
            osg::Material* mat = static_cast<osg::Material*>(stateset->getAttribute(osg::StateAttribute::MATERIAL));
            switch (mTargetColor)
            {
                case Diffuse:
                {
                    osg::Vec4f diffuse = mat->getDiffuse(osg::Material::FRONT_AND_BACK);
                    diffuse.set(value.x(), value.y(), value.z(), diffuse.a());
                    mat->setDiffuse(osg::Material::FRONT_AND_BACK, diffuse);
                    break;
                }
                case Specular:
                {
                    osg::Vec4f specular = mat->getSpecular(osg::Material::FRONT_AND_BACK);
                    specular.set(value.x(), value.y(), value.z(), specular.a());
                    mat->setSpecular(osg::Material::FRONT_AND_BACK, specular);
                    break;
                }
                case Emissive:
                {
                    osg::Vec4f emissive = mat->getEmission(osg::Material::FRONT_AND_BACK);
                    emissive.set(value.x(), value.y(), value.z(), emissive.a());
                    mat->setEmission(osg::Material::FRONT_AND_BACK, emissive);
                    break;
                }
                case Ambient:
                default:
                {
                    osg::Vec4f ambient = mat->getAmbient(osg::Material::FRONT_AND_BACK);
                    ambient.set(value.x(), value.y(), value.z(), ambient.a());
                    mat->setAmbient(osg::Material::FRONT_AND_BACK, ambient);
                }
            }
        }
    }

    FlipController::FlipController(
        const Nif::NiFlipController* ctrl, const std::vector<osg::ref_ptr<osg::Texture2D>>& textures)
        : mTexSlot(0) // always affects diffuse
        , mDelta(ctrl->mDelta)
        , mTextures(textures)
    {
        if (!ctrl->mInterpolator.empty() && ctrl->mInterpolator->recType == Nif::RC_NiFloatInterpolator)
            mData = static_cast<const Nif::NiFloatInterpolator*>(ctrl->mInterpolator.getPtr());
    }

    FlipController::FlipController(int texSlot, float delta, const std::vector<osg::ref_ptr<osg::Texture2D>>& textures)
        : mTexSlot(texSlot)
        , mDelta(delta)
        , mTextures(textures)
    {
    }

    FlipController::FlipController(const FlipController& copy, const osg::CopyOp& copyop)
        : StateSetUpdater(copy, copyop)
        , Controller(copy)
        , mTexSlot(copy.mTexSlot)
        , mDelta(copy.mDelta)
        , mTextures(copy.mTextures)
        , mData(copy.mData)
    {
    }

    void FlipController::apply(osg::StateSet* stateset, osg::NodeVisitor* nv)
    {
        if (hasInput() && !mTextures.empty())
        {
            int curTexture = 0;
            if (mDelta != 0)
                curTexture = int(getInputValue(nv) / mDelta) % mTextures.size();
            else
                curTexture = int(mData.interpKey(getInputValue(nv))) % mTextures.size();
            stateset->setTextureAttribute(mTexSlot, mTextures[curTexture]);
        }
    }

    ParticleSystemController::ParticleSystemController(const Nif::NiParticleSystemController* ctrl)
        : mEmitStart(ctrl->startTime)
        , mEmitStop(ctrl->stopTime)
    {
    }

    ParticleSystemController::ParticleSystemController()
        : mEmitStart(0.f)
        , mEmitStop(0.f)
    {
    }

    ParticleSystemController::ParticleSystemController(const ParticleSystemController& copy, const osg::CopyOp& copyop)
        : SceneUtil::NodeCallback<ParticleSystemController, osgParticle::ParticleProcessor*>(copy, copyop)
        , Controller(copy)
        , mEmitStart(copy.mEmitStart)
        , mEmitStop(copy.mEmitStop)
    {
    }

    void ParticleSystemController::operator()(osgParticle::ParticleProcessor* node, osg::NodeVisitor* nv)
    {
        if (hasInput())
        {
            float time = getInputValue(nv);
            node->getParticleSystem()->setFrozen(false);
            node->setEnabled(time >= mEmitStart && time < mEmitStop);
        }
        else
            node->getParticleSystem()->setFrozen(true);
        traverse(node, nv);
    }

    PathController::PathController(const PathController& copy, const osg::CopyOp& copyop)
        : SceneUtil::NodeCallback<PathController, NifOsg::MatrixTransform*>(copy, copyop)
        , Controller(copy)
        , mPath(copy.mPath)
        , mPercent(copy.mPercent)
        , mFlags(copy.mFlags)
    {
    }

    PathController::PathController(const Nif::NiPathController* ctrl)
        : mPath(ctrl->posData->mKeyList, osg::Vec3f())
        , mPercent(ctrl->floatData->mKeyList, 1.f)
        , mFlags(ctrl->flags)
    {
    }

    float PathController::getPercent(float time) const
    {
        float percent = mPercent.interpKey(time);
        if (percent < 0.f)
            percent = std::fmod(percent, 1.f) + 1.f;
        else if (percent > 1.f)
            percent = std::fmod(percent, 1.f);
        return percent;
    }

    void PathController::operator()(NifOsg::MatrixTransform* node, osg::NodeVisitor* nv)
    {
        if (mPath.empty() || mPercent.empty() || !hasInput())
        {
            traverse(node, nv);
            return;
        }

        float time = getInputValue(nv);
        float percent = getPercent(time);
        node->setTranslation(mPath.interpKey(percent));

        traverse(node, nv);
    }

}
