require_relative 'helpers'
require 'orocos'
Orocos.load_typekit 'base'

module Vizkit
    describe 'SonarVisualization' do
        include TestHelpers

        before do
            register_widget(@vizkit3d_widget =
                Vizkit.default_loader.create_plugin("vizkit3d::Vizkit3DWidget"))
            @ocean_viz = @vizkit3d_widget.createPlugin('vizkit3d_ocean', 'Ocean')
            @ocean_viz.setUwFogDensity(0.3);

            rbs = Types.base.samples.RigidBodyState.Invalid
            rbs.position = Eigen::Vector3.new(15, 15, -5)
            @rbs_viz = @vizkit3d_widget.createPlugin('base', 'RigidBodyStateVisualization')
            @rbs_viz.updateData(rbs)

            @vizkit3d_widget.setEnvironmentPlugin(@ocean_viz)
            @vizkit3d_widget.setCameraEye(10, 10, -5)
            @vizkit3d_widget.setCameraLookAt(20, 20, -5)
            @vizkit3d_widget.show
        end

        it "works by default" do
            confirm 'It looks like an underwater environment'
        end

        it "parametrizes the underwater fog" do
            @ocean_viz.setUwFogDensity(0.3)
            @ocean_viz.setUwFogColor(Qt::Color.new(200, 100, 100))
            confirm 'You should see a 3-axis model with a red tint'
            @ocean_viz.setUwFogDensity(0.4)
            @ocean_viz.setUwFogColor(Qt::Color.new(27, 57, 109))
            confirm 'The RBS should be even more faint'
            @ocean_viz.setUwFogDensity(0)
            confirm 'The RBS should not be blurred at all'
        end

        it "parametrizes the underwater attenuation" do
            @ocean_viz.setUwFogDensity(0)
            @ocean_viz.setUwAttenuation(Qt::Vector3D.new(0, 0.5, 0.5))
            confirm 'There should be a bizarrely red tint'
        end

        it "parametrizes the underwater diffuse color" do
            step 'Look at the difference between this and the next step. The axes should get a red tint'
            @ocean_viz.setUwDiffuseColor(Qt::Color.new(255, 0, 0))
            confirm 'Did the axes get a red tint ?'
        end
    end
end

