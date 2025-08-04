from collections import deque
from logic.maze import Maze

class MazeSolver:
    def __init__(self, maze):
        self.maze = maze

    
    def solve(self):
        if not self.maze.start_pos or not self.maze.end_pos:
            return None

        # Initialize queue and visited set
        queue = deque([(self.maze.start_pos, [self.maze.start_pos])])
        visited = {self.maze.start_pos}

        while queue:
            current_pos, path = queue.popleft()
            
            if current_pos == self.maze.end_pos:
                # Return the sequence of positions
                return [(pos[0] + 1, pos[1] + 1) for pos in path]  # Convert to 1-based coordinates

            # Get all possible moves from current position
            for next_row, next_col, _ in self.maze.get_neighbors(*current_pos):
                next_pos = (next_row, next_col)
                
                if next_pos not in visited:
                    visited.add(next_pos)
                    new_path = path + [next_pos]
                    queue.append((next_pos, new_path))

        return None  # No path found
  