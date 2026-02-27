import carla
import time
import sys

def main():
    print("Connecting to CARLA...")
    client = carla.Client('127.0.0.1', 2000)
    client.set_timeout(10.0)
    world = client.get_world()
    
    print("Waiting for vehicle to spawn...")
    vehicle = None
    for _ in range(20):
        actors = world.get_actors().filter('vehicle.*')
        if len(actors) > 0:
            vehicle = actors[0]
            break
        time.sleep(1)
        
    if not vehicle:
        print("Vehicle never spawned.")
        sys.exit(1)
        
    print(f"Tracking {vehicle.type_id}...")
    
    start_loc = vehicle.get_location()
    print(f"T=0: Location({start_loc.x}, {start_loc.y}, {start_loc.z})")
    
    time.sleep(3)
    
    end_loc = vehicle.get_location()
    print(f"T=3: Location({end_loc.x}, {end_loc.y}, {end_loc.z})")
    
    distance = start_loc.distance(end_loc)
    print(f"Translation Distance: {distance:.2f} meters")
    
    if distance > 0.5:
        print("SUCCESS: The car is actively moving.")
    else:
        print("FAILURE: The car forms a static trajectory.")
        
if __name__ == '__main__':
    main()
