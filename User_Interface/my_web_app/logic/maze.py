from logic.square import Square

class Maze:
    def __init__(self, size=10):
        self.size = size
        self.grid = [[Square() for _ in range(size)] for _ in range(size)]
        self.start_pos = None
        self.end_pos = None
        self.robot_head_coardinates = 0
        #self.solution = None
        #self.directions_list = []

    def make_directions_list(self, path):
        if not path or len(path) < 2:
            return []

        directions_list = []
        for i in range(len(path) - 1): 
            current_pos = path[i]
            next_pos = path[i + 1]

            #print(current_pos[0], current_pos[1])
            
            # Convert 1-based coordinates to 0-based for grid access
            current_row = current_pos[0] - 1
            current_col = current_pos[1] - 1
            current_square = self.get_square(current_row, current_col)
            
            # Determine the movement direction
            dx = next_pos[0] - current_pos[0]  # Change in x (columns)
            dy = next_pos[1] - current_pos[1]  # Change in y (rows)
            
            # Skip regular squares (they only have forward/backward)
            if current_square.regular or i==0:
                continue
            
            #robot coardinates statets
            if self.robot_head_coardinates == 0:
                if dx == 1:
                    directions_list.append("forward")
                if dx == -1:
                    print("wrong- robot cannot go backward")
                    break
                if dy == 1:
                    directions_list.append("left")
                    self.robot_head_coardinates = 3
                if dy == -1:
                    directions_list.append("right")
                    self.robot_head_coardinates = 1

            elif self.robot_head_coardinates == 1:
                if dx == 1:
                    directions_list.append("left")
                    self.robot_head_coardinates = 0
                if dx == -1:
                    directions_list.append("right")
                    self.robot_head_coardinates = 2
                if dy == 1:
                    print("wrong- robot cannot go backward")
                    break
                if dy == -1:
                    directions_list.append("forward")

            elif self.robot_head_coardinates == 2:
                if dx == 1:
                    print("wrong- robot cannot go backward")
                    break
                if dx == -1:
                    directions_list.append("forward")
                if dy == 1:
                    directions_list.append("right")
                    self.robot_head_coardinates = 3
                if dy == -1:
                    directions_list.append("left")
                    self.robot_head_coardinates = 1

            elif self.robot_head_coardinates == 3:
                if dx == 1:
                    directions_list.append("right")
                    self.robot_head_coardinates = 0
                if dx == -1:
                    directions_list.append("left")
                    self.robot_head_coardinates = 2
                if dy == 1:
                    directions_list.append("forward")
                if dy == -1:
                    print("wrong- robot cannot go backward")
                    break
            
            # if directions_list:
            #     print("Last direction:", directions_list[-1])

        # If you want to see the last direction, use:
        
        return directions_list

    def set_square_directions(self, row, col, up=False, down=False, left=False, right=False):
        if 0 <= row < self.size and 0 <= col < self.size:
            self.grid[row][col].set_directions(up, down, left, right)
        self.grid[row][col].set_regular()

    def set_start_position(self, row, col):
        if 0 <= row < self.size and 0 <= col < self.size:
            self.start_pos = (row, col)

    def set_end_position(self, row, col):
        if 0 <= row < self.size and 0 <= col < self.size:
            self.end_pos = (row, col)

    def get_square(self, row, col):
        if 0 <= row < self.size and 0 <= col < self.size:
            return self.grid[row][col]
        return None

    def is_valid_position(self, row, col):
        return 0 <= row < self.size and 0 <= col < self.size

    def get_neighbors(self, row, col):
        neighbors = []
        current_square = self.get_square(row, col)
        
        if current_square.is_blocked():
            return neighbors

        # Define all possible directions
        directions = {
            'up': (-1, 0),
            'down': (1, 0),
            'left': (0, -1),
            'right': (0, 1)
        }

        # Get possible moves for current square
        possible_moves = current_square.get_possible_moves()

        # Check each possible direction
        for move in possible_moves:
            if move in directions:
                dx, dy = directions[move]
                new_row, new_col = row + dx, col + dy
                
                if self.is_valid_position(new_row, new_col):
                    neighbor = self.get_square(new_row, new_col)
                    if not neighbor.is_blocked():
                        neighbors.append((new_row, new_col, move))

        return neighbors

    def display(self):
        # Print top border
        print("+" + "---+" * self.size)
        
        for row in range(self.size):
            # Print top walls and cells
            print("|", end="")
            for col in range(self.size):
                square = self.grid[row][col]
                # Print cell content
                if (row, col) == self.start_pos:
                    print(" S ", end="")
                elif (row, col) == self.end_pos:
                    print(" E ", end="")
                else:
                    print("   ", end="")
                # Print right wall
                print("|" if not square.right else " ", end="")
            print()
            
            # Print bottom walls
            print("+", end="")
            for col in range(self.size):
                square = self.grid[row][col]
                # Print bottom wall
                print("---+" if not square.down else "   +", end="")
            print() 