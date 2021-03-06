#include <vizkit3d/Ocean.hpp>
#include <vizkit3d_ocean/SkyDome.hpp>

#include <osgOcean/FFTOceanSurface>
#include <osgOcean/OceanScene>
#include <osgOcean/ShaderManager>
#include <osgDB/FileUtils>
#include <osg/LightSource>

using namespace vizkit3d;
using namespace osg;
using namespace osgOcean;

class CameraTrackCallback: public osg::NodeCallback
{
public:
    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
    {
        if( nv->getVisitorType() == osg::NodeVisitor::CULL_VISITOR )
        {
            osgUtil::CullVisitor* cv = static_cast<osgUtil::CullVisitor*>(nv);
            osg::Vec3f centre,up,eye;
            // get MAIN camera eye,centre,up
            cv->getRenderStage()->getCamera()->getViewMatrixAsLookAt(eye,centre,up);
            // update position
            osg::MatrixTransform* mt = static_cast<osg::MatrixTransform*>(node);
            mt->setMatrix( osg::Matrix::translate( eye.x(), eye.y(), mt->getMatrix().getTrans().z() ) );
        }

        traverse(node, nv);
    }
};

static osg::Vec4f qt2osg(QColor const& color)
{
    return osg::Vec4f(color.redF(), color.greenF(), color.blueF(), 1.0f);
}
static osg::Vec3f qt2osg_color3(QColor const& color)
{
    return osg::Vec3f(color.redF(), color.greenF(), color.blueF());
}
static osg::Vec2f qt2osg(QVector2D const& v)
{
    return osg::Vec2f(v.x(), v.y());
}
static osg::Vec3f qt2osg(QVector3D const& v)
{
    return osg::Vec3f(v.x(), v.y(), v.z());
}


Ocean::Ocean(const OceanParameters& ocean_parameters)
    : cubeMapPath("vizkit3d_ocean/textures/sky_clear")
{
    surfEndless = ocean_parameters.surfEndless;
    surfWaveScale = ocean_parameters.surfWaveScale;
    surfWaveBottomColor = ocean_parameters.surfWaveBottomColor;
    surfWaveTopColor = ocean_parameters.surfWaveTopColor;
    surfDepth = ocean_parameters.surfDepth;
    surfWindDirection = ocean_parameters.surfWindDirection;
    surfWindSpeed = ocean_parameters.surfWindSpeed;
    surfReflectionDamping = ocean_parameters.surfReflectionDamping;
    surfIsChoppy = ocean_parameters.surfIsChoppy;
    surfChoppyFactor = ocean_parameters.surfChoppyFactor;
    surfCrestFoam = ocean_parameters.surfCrestFoam;
    surfCrestFoamHeight = ocean_parameters.surfCrestFoamHeight;
    surfFoamBottomHeight = ocean_parameters.surfFoamBottomHeight;
    surfFoamTopHeight = ocean_parameters.surfFoamTopHeight;
    surfHeight = ocean_parameters.surfHeight;

    airFogColor = ocean_parameters.airFogColor;
    airFogDensity = ocean_parameters.airFogDensity;
    sunPosition = ocean_parameters.sunPosition;
    sunDiffuseColor = ocean_parameters.sunDiffuseColor;
    sunColor = ocean_parameters.sunColor;
    uwFogColor = ocean_parameters.uwFogColor;
    uwFogDensity = ocean_parameters.uwFogDensity;
    uwAttenuation = ocean_parameters.uwAttenuation;
    uwDiffuseColor = ocean_parameters.uwDiffuseColor;
    glareAttenuation = ocean_parameters.glareAttenuation;

    reflections = ocean_parameters.reflections;
    refractions = ocean_parameters.refractions;
    heightmap = ocean_parameters.heightmap;
    godRays = ocean_parameters.godRays;
    silt = ocean_parameters.silt;
    underwaterDOF = ocean_parameters.underwaterDOF;
    underwaterScattering = ocean_parameters.underwaterScattering;
    distortion = ocean_parameters.distortion;
    glare = ocean_parameters.glare;

    loadCubeMapImages(cubeMapPath);
    ref_node = new osg::Group;
}

Ocean::~Ocean()
{
}

ref_ptr<Group> Ocean::getRefNode()
{
    return ref_node;
}

ref_ptr<Node> Ocean::createMainNode()
{
    TextureCubeMap* cubeMap = createCubeMap();
    updateCubeMap(cubeMap);
    FFTOceanTechnique* surface = createSurface();
    updateSurface(surface, cubeMap);
    OceanScene* scene = createScene(surface);
    updateScene(scene);
    vizkit3d_ocean::SkyDome* dome = createSkyDome(cubeMap);
    updateSkyDome(dome, scene);
    LightSource* light = createLight();
    updateLight(light);

    Group* mainNode = new Group;
    mainNode->addChild(scene);
    scene->addChild(ref_node);
    mainNode->addChild(light);

    osg::ref_ptr<osg::StateSet> state = mainNode->getOrCreateStateSet();
    state->setGlobalDefaults();
    state->setDataVariance(osg::Object::DYNAMIC);

    // add a pat to track the camera
    MatrixTransform* transform = new MatrixTransform;
    transform->setDataVariance( Object::DYNAMIC );
    transform->setMatrix(Matrixf::translate( Vec3f(0.f, 0.f, 0.f) ));
    transform->setCullCallback( new CameraTrackCallback );
    transform->addChild(dome);
    scene->addChild(transform);

    ref_node->setNodeMask(scene->getNormalSceneMask() |
            scene->getReflectedSceneMask() |
            scene->getRefractedSceneMask());

    osgOcean::ShaderManager::instance().enableShaders(true);

    return mainNode;
}

void Ocean::loadCubeMapImages(QString dir)
{
    std::string filenames[6];
    filenames[0] = osgDB::findDataFile(dir.toStdString() + "/west.png");
    filenames[1] = osgDB::findDataFile(dir.toStdString() + "/east.png");
    filenames[2] = osgDB::findDataFile(dir.toStdString() + "/up.png");
    filenames[3] = osgDB::findDataFile(dir.toStdString() + "/down.png");
    filenames[4] = osgDB::findDataFile(dir.toStdString() + "/south.png");
    filenames[5] = osgDB::findDataFile(dir.toStdString() + "/north.png");

    ref_ptr<Image> images[6];
    for (int i = 0; i < 6; ++i)
    {
        if (filenames[i].empty())
            throw std::runtime_error("cannot find required cubemap texture in " + dir.toStdString());
        images[i] = osgDB::readImageFile( filenames[i] );
    }

    for (int i = 0; i < 6; ++i)
        cubeMapImages[i] = images[i];
}

TextureCubeMap* Ocean::createCubeMap()
{
    TextureCubeMap* cubeMap = new TextureCubeMap;
    cubeMap->setInternalFormat(GL_RGBA);
    cubeMap->setFilter(Texture::MIN_FILTER, Texture::LINEAR_MIPMAP_LINEAR);
    cubeMap->setFilter(Texture::MAG_FILTER, Texture::LINEAR);
    cubeMap->setWrap(Texture::WRAP_S, Texture::CLAMP_TO_EDGE);
    cubeMap->setWrap(Texture::WRAP_T, Texture::CLAMP_TO_EDGE);
    return cubeMap;
}

void Ocean::updateCubeMap(TextureCubeMap* cubeMap)
{
    cubeMap->setImage(TextureCubeMap::NEGATIVE_X, cubeMapImages[0]);
    cubeMap->setImage(TextureCubeMap::POSITIVE_X, cubeMapImages[1]);
    cubeMap->setImage(TextureCubeMap::NEGATIVE_Y, cubeMapImages[2]);
    cubeMap->setImage(TextureCubeMap::POSITIVE_Y, cubeMapImages[3]);
    cubeMap->setImage(TextureCubeMap::NEGATIVE_Z, cubeMapImages[4]);
    cubeMap->setImage(TextureCubeMap::POSITIVE_Z, cubeMapImages[5]);
}

osgOcean::FFTOceanTechnique* Ocean::createSurface()
{
    osgOcean::FFTOceanTechnique* surface =
        new osgOcean::FFTOceanSurface( 64, 256, 17,
                qt2osg(surfWindDirection),
                surfWindSpeed,
                surfDepth,
                surfReflectionDamping,
                surfWaveScale / 1e8,
                surfIsChoppy,
                surfChoppyFactor,
                10.f, 256 );
    surface->setWaveTopColor(qt2osg_color3(surfWaveTopColor));
    surface->setWaveBottomColor(qt2osg_color3(surfWaveBottomColor));
    return surface;
}

void Ocean::updateSurface(osgOcean::FFTOceanTechnique* surface, TextureCubeMap* cubeMap)
{
    surface->setEnvironmentMap( cubeMap );
    surface->setFoamBottomHeight( surfFoamBottomHeight );
    surface->setFoamTopHeight( surfFoamTopHeight );
    surface->enableCrestFoam(surfCrestFoam);
    surface->setLightColor(qt2osg(sunColor));
    surface->enableEndlessOcean(surfEndless);
}

osgOcean::OceanScene* Ocean::createScene(osgOcean::FFTOceanTechnique* surface)
{
    osgOcean::OceanScene* scene =
        new osgOcean::OceanScene(surface);

    // Create and add fake texture for use with nodes without any texture
    // since the OceanScene default scene shader assumes that texture unit
    // 0 is used as a base texture map.
    Image * image = new Image();
    image->allocateImage( 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE );
    *(Vec4ub*)image->data() = Vec4ub( 0xFF, 0xFF, 0xFF, 0xFF );

    Texture2D* fakeTex = new Texture2D( image );
    fakeTex->setWrap(Texture2D::WRAP_S,Texture2D::REPEAT);
    fakeTex->setWrap(Texture2D::WRAP_T,Texture2D::REPEAT);
    fakeTex->setFilter(Texture2D::MIN_FILTER,Texture2D::NEAREST);
    fakeTex->setFilter(Texture2D::MAG_FILTER,Texture2D::NEAREST);

    StateSet* stateset = scene->getOrCreateStateSet();
    stateset->setTextureAttribute(0,fakeTex,StateAttribute::ON);
    stateset->setTextureMode(0,GL_TEXTURE_1D,StateAttribute::OFF);
    stateset->setTextureMode(0,GL_TEXTURE_2D,StateAttribute::ON);
    stateset->setTextureMode(0,GL_TEXTURE_3D,StateAttribute::OFF);

    return scene;
}

void Ocean::updateScene(osgOcean::OceanScene* scene)
{
    scene->setLightID(0);
    scene->enableReflections(reflections);
    scene->enableRefractions(refractions);
    scene->enableHeightmap(heightmap);

    scene->setCylinderSize( 1900.f, 4000.f );

    scene->setAboveWaterFog(airFogDensity / 1000, qt2osg(airFogColor) );
    scene->setUnderwaterFog(uwFogDensity, qt2osg(uwFogColor) );
    scene->setUnderwaterDiffuse(qt2osg(uwDiffuseColor));
    scene->setUnderwaterAttenuation(qt2osg(uwAttenuation));
    scene->setOceanSurfaceHeight(surfHeight);

    Vec3f sunDir = qt2osg(-sunPosition);
    sunDir.normalize();
    scene->setSunDirection( sunDir );

    scene->enableGodRays(godRays);
    scene->enableSilt(silt);
    scene->enableUnderwaterDOF(underwaterDOF);
    scene->enableUnderwaterScattering(underwaterScattering);
    scene->enableDistortion(distortion);
    scene->enableGlare(glare);
    scene->setGlareAttenuation(glareAttenuation);
}

LightSource* Ocean::createLight()
{
    LightSource* lightSource = new LightSource();
    lightSource->setLocalStateSetModes();
    lightSource->getLight()->setLightNum(0);
    return lightSource;
}

void Ocean::updateLight(LightSource* lightSource)
{
    osg::Light* light = lightSource->getLight();
    light->setAmbient( osg::Vec4d(0.3f, 0.3f, 0.3f, 1.0f ));
    light->setDiffuse(qt2osg(sunDiffuseColor));
    light->setSpecular(osg::Vec4d( 0.1f, 0.1f, 0.1f, 1.0f ) );

    osg::Vec3f direction(qt2osg(sunPosition));
    direction.normalize();
    light->setPosition( osg::Vec4f(direction, 0.0) );
}

vizkit3d_ocean::SkyDome* Ocean::createSkyDome(TextureCubeMap* cubeMap)
{
    vizkit3d_ocean::SkyDome* skyDome = new vizkit3d_ocean::SkyDome( 1900.f, 16, 16, cubeMap );
    return skyDome;
}

void Ocean::updateSkyDome(vizkit3d_ocean::SkyDome* dome, OceanScene* scene)
{
    dome->setNodeMask(
            scene->getReflectedSceneMask() |
            scene->getNormalSceneMask()    |
            scene->getRefractedSceneMask());
}

void Ocean::updateMainNode( Node* node )
{
    osg::Group* group = dynamic_cast<Group*>(node);
    osgOcean::OceanScene* scene = dynamic_cast<osgOcean::OceanScene*>(group->getChild(0));
    updateScene(scene);

    FFTOceanTechnique* surface =
        dynamic_cast<FFTOceanTechnique*>(scene->getOceanTechnique());
    surface->setLightColor(qt2osg(sunColor));
    surface->setWaveTopColor(qt2osg_color3(surfWaveTopColor));
    surface->setWaveBottomColor(qt2osg_color3(surfWaveBottomColor));
    surface->setFoamBottomHeight( surfFoamBottomHeight );
    surface->setFoamTopHeight( surfFoamTopHeight );
    surface->setWaveScaleFactor(surfWaveScale / 1e8);
}

void Ocean::updateDataIntern(base::Vector3d const& value)
{
}

void Ocean::setAirFogColor(QColor const& color)
{
    airFogColor = color;
    emit propertyChanged("airFogColor");
    setDirty();
}

QColor Ocean::getAirFogColor() const
{
    return airFogColor;
}

void Ocean::setAirFogDensity(double density)
{
    airFogDensity = density;
    emit propertyChanged("airFogDensity");
    setDirty();
}

double Ocean::getAirFogDensity() const
{
    return airFogDensity;
}

void Ocean::setUwFogColor(QColor const& color)
{
    uwFogColor = color;
    emit propertyChanged("uwFogColor");
    setDirty();
}

QColor Ocean::getUwFogColor() const
{
    return uwFogColor;
}

void Ocean::setUwFogDensity(double density)
{
    uwFogDensity = density;
    emit propertyChanged("uwFogDensity");
    setDirty();
}

double Ocean::getUwFogDensity() const
{
    return uwFogDensity;
}

void Ocean::setUwAttenuation(QVector3D const& factors)
{
    uwAttenuation = factors;
    emit propertyChanged("uwAttenuation");
    setDirty();
}

QVector3D Ocean::getUwAttenuation() const
{
    return uwAttenuation;
}

void Ocean::setUwDiffuseColor(QColor const& factors)
{
    uwDiffuseColor = factors;
    emit propertyChanged("uwDiffuseColor");
    setDirty();
}

QColor Ocean::getUwDiffuseColor() const
{
    return uwDiffuseColor;
}

void Ocean::setGlareAttenuation(double density)
{
    glareAttenuation = density;
    emit propertyChanged("glareAttenuation");
    setDirty();
}

double Ocean::getGlareAttenuation() const
{
    return glareAttenuation;
}

void Ocean::setWaveScale(double scale)
{
    surfWaveScale = scale;
    emit propertyChanged("waveScale");
    setDirty();
}

double Ocean::getWaveScale() const
{
    return surfWaveScale;
}

void Ocean::setSunColor(QColor const& color)
{
    sunColor = color;
    emit propertyChanged("sunColor");
    setDirty();
}

QColor Ocean::getSunColor() const
{
    return sunColor;
}

void Ocean::setWaveTopColor(QColor const& color)
{
    surfWaveTopColor = color;
    emit propertyChanged("waveTopColor");
    setDirty();
}

QColor Ocean::getWaveTopColor() const
{
    return surfWaveTopColor;
}

void Ocean::setFoamTopHeight(double height)
{
    surfFoamTopHeight = height;
    emit propertyChanged("foamTopHeight");
    setDirty();
}

double Ocean::getFoamTopHeight() const
{
    return surfFoamTopHeight;
}

void Ocean::setFoamBottomHeight(double height)
{
    surfFoamBottomHeight = height;
    emit propertyChanged("foamBottomHeight");
    setDirty();
}

double Ocean::getFoamBottomHeight() const
{
    return surfFoamBottomHeight;
}

void Ocean::setWaveBottomColor(QColor const& color)
{
    surfWaveBottomColor = color;
    emit propertyChanged("waveBottomColor");
    setDirty();
}

QColor Ocean::getWaveBottomColor() const
{
    return surfWaveBottomColor;
}

double Ocean::getSurfaceHeight() const
{
    return surfHeight;
}

void Ocean::setSurfaceHeight(double height)
{
    surfHeight = height;
    emit propertyChanged("surfaceHeight");
    setDirty();
}

VizkitQtPlugin(Ocean)

