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

            @vizkit3d_widget.setEnvironmentPlugin(@ocean_viz)
            @vizkit3d_widget.setCameraEye(10, 10, 2)
            @vizkit3d_widget.setCameraLookAt(20, 20, 0)
            @vizkit3d_widget.show
        end

        it "simulates the waves" do
            @ocean_viz.setAirFogDensity(1)
            @ocean_viz.setWaveScale(0)
            confirm 'You should see a water surface with no waves'
            @ocean_viz.setWaveScale(20)
            confirm 'There are waves now'
            @ocean_viz.setFoamBottomHeight(0)
            confirm 'The waves\'top is more white now'
            @ocean_viz.setFoamTopHeight(10)
            confirm 'The waves\'top is softer'
        end

        it "simulates the fog" do
            confirm 'You should see a water surface with small waves'
            @ocean_viz.setAirFogDensity(50)
            confirm 'The air fog should be much stronger now'
            @ocean_viz.setAirFogDensity(10)
            confirm 'The air fog should be much lighter now'
            @ocean_viz.setAirFogDensity(25)
            @ocean_viz.setAirFogColor(Qt::Color.new(200, 100, 100))
            confirm 'The air fog should be reddish'
        end

        it "simulates the sun" do
            @ocean_viz.setSunColor(Qt::Color.new(200, 100, 100))
            confirm 'The environment should have a red tint'
        end
    end
end

