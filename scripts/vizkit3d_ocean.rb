require 'vizkit'
Orocos.initialize

view3d = Vizkit.vizkit3d_widget
ocean = Vizkit.default_loader.Ocean
flat_fish = Vizkit.default_loader.RigidBodyStateVisualization
view3d.setGrid(false)
view3d.setCameraManipulator("Default")
view3d.setEnvironmentPlugin(ocean)

view3d.setCameraEye(0, 0, 20)
view3d.setCameraLookAt(0, 1, 20)
view3d.setCameraUp(0, 0, 1)
view3d.show
Vizkit.exec
