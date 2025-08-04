from enum import Enum

class Square:
    def __init__(self):
        # Initialize all directions as walls (False means wall, True means empty)
        self.up = False
        self.down = False
        self.left = False
        self.right = False
        self.visited = False
        self.regular = False
    
    def set_regular(self):
        if self.up and self.down and not self.left and not self.right:
            self.regular = True
        if not self.up and not self.down and self.left and self.right:
            self.regular = True

    def set_directions(self, up=False, down=False, left=False, right=False):
        self.up = up
        self.down = down
        self.left = left
        self.right = right

    def get_possible_moves(self):
        moves = []
        if self.up:
            moves.append('up')
        if self.down:
            moves.append('down')
        if self.left:
            moves.append('left')
        if self.right:
            moves.append('right')
        return moves

    def is_blocked(self):
        return not any([self.up, self.down, self.left, self.right]) 