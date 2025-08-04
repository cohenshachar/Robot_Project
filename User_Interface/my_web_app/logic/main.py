from logic.maze import Maze
from logic.maze_solver import MazeSolver

def create_maze(maze_grid, start_pos, end_pos):
    maze = Maze(10)
    
    # print("Create your maze (3x3):")
    # print("For each cell, specify which directions are open (no walls):")
    # print("Enter 4 characters for each cell:")
    # print("U - Up is open")
    # print("D - Down is open")
    # print("L - Left is open")
    # print("R - Right is open")
    # print("Example: 'UDRL' means Up, Down, Right, Left are all open")
    # print("Enter 'X' for a completely blocked cell")
    # print("\nEnter the maze row by row (3 cells per row):")
    
    for i in range(10):
        for j in range(10):
            while True:
                # cell = input(f"Cell ({i+1},{j+1}) directions (e.g., UDRL or X): ").strip().upper()
                cell = maze_grid[i][j].upper()
                if cell == 'X':
                    maze.set_square_directions(i, j, False, False, False, False)
                    break
                elif len(cell) <= 4 and all(c in 'UDRL' for c in cell):
                    up = 'U' in cell
                    down = 'D' in cell
                    left = 'L' in cell
                    right = 'R' in cell
                    maze.set_square_directions(i, j, up, down, left, right)
                    break
                else:
                    # print("Invalid input! Please enter up to 4 characters (U,D,L,R) or X")
                    break
    # Get start position
    start_row, start_col = start_pos
    start_row = start_row - 1
    start_col = start_col - 1

    end_row, end_col = end_pos
    end_row = end_row - 1
    end_col = end_col - 1

    maze.set_start_position(start_row, start_col)
    maze.set_end_position(end_row, end_col)
    # while True:
    #     try:
    #         start_row = int(input("Enter start row (1-3): ")) - 1
    #         start_col = int(input("Enter start column (1-3): ")) - 1
    #         if 0 <= start_row < 3 and 0 <= start_col < 3:
    #             if not maze.get_square(start_row, start_col).is_blocked():
    #                 maze.set_start_position(start_row, start_col)
    #                 break
    #             else:
    #                 print("Cannot place start position on a blocked square!")
    #         else:
    #             print("Invalid position! Please enter numbers between 1 and 3")
    #     except ValueError:
    #         print("Please enter valid numbers!")
    
    # Get end position
    # while True:
    #     try:
    #         end_row = int(input("Enter end row (1-3): ")) - 1
    #         end_col = int(input("Enter end column (1-3): ")) - 1
    #         if 0 <= end_row < 3 and 0 <= end_col < 3:
    #             if not maze.get_square(end_row, end_col).is_blocked():
    #                 maze.set_end_position(end_row, end_col)
    #                 break
    #             else:
    #                 print("Cannot place end position on a blocked square!")
    #         else:
    #             print("Invalid position! Please enter numbers between 1 and 3")
    #     except ValueError:
    #         print("Please enter valid numbers!")
    
    return maze


def solve_maze_from_web(maze_grid , start_pos , end_pos):
    # Create the maze
    maze = create_maze(maze_grid, start_pos, end_pos)
    # Solve the maze
    solver = MazeSolver(maze)
    solution = solver.solve()

    if solution:
        directions = maze.make_directions_list(solution)
        return {
            "status": "ok",
            "path": solution,
            "directions": directions
        }
    else:
        return {
            "status": "no_solution",
            "message": "Maze could not be solved."
        }
    

def main():
    print("Welcome to the Maze Solver!")
    print("===========================")
    
    # Create the maze
    maze = create_maze()
    
    # Display the maze
    print("\nYour maze:")
    maze.display()
    for i in range(10):
        for j in range(10):
            if maze.get_square(i, j).regular:
                print(f"Regular square: ({i+1}, {j+1})")
    
    # Solve the maze
    solver = MazeSolver(maze)
    solution = solver.solve()
    
    if solution:
        #maze.solution = solution
        print("\nSolution found!")
        print("Path (sequence of positions):")
        for i, pos in enumerate(solution):
            print(f"Step {i+1}: ({pos[0]}, {pos[1]})")
        
        # Get the directions list for non-regular squares
        directions = maze.make_directions_list(solution)
        if directions:
            print("\nRequired turns at non-regular squares:")
            for direction in directions:
                print(f"Turn {direction}")
    else:
        print("\nNo solution found!")

# if __name__ == "__main__":
#     main() 