import pygame
import subprocess
import sys
import time
import math

# Configuration
ENGINE_PATH = './3d-connect4'
WINDOW_SIZE = (1000, 800)

# Colors
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
BG_COLOR = (30, 30, 30)
RED = (220, 20, 20)
YELLOW = (220, 220, 20)
BLUE = (0, 0, 220)
GRAY = (200, 200, 200)
BROWN = (139, 69, 19)
DARK_BROWN = (80, 40, 10)

class Connect43D:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode(WINDOW_SIZE)
        pygame.display.set_caption("3D Connect 4")
        self.font = pygame.font.SysFont('Arial', 24)
        self.large_font = pygame.font.SysFont('Arial', 48)
        
        # 3D Camera State
        self.angle_y = math.pi / 4  # Horizontal rotation (45 deg)
        self.angle_x = -math.pi / 6 # Vertical tilt (-30 deg)
        self.camera_dist = 600
        self.fov = 500

        self.start_engine()
        self.board_a = 0
        self.board_b = 0
        self.turn = 1 # 1=A (Red), 2=B (Yellow)
        self.game_over = False
        self.winner = None
        
        # Mode: 0=HvH, 1=HvAI, 2=AIvAI
        self.mode = 0 
        self.ai_depth = 6
        
        self.hover_rc = None # (r, c) of column under mouse

        self.select_mode_screen()

    def start_engine(self):
        try:
            self.engine = subprocess.Popen(
                [ENGINE_PATH, 'interactive'],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1
            )
        except FileNotFoundError:
            print(f"Error: Could not find executable '{ENGINE_PATH}'. Did you compile it?")
            sys.exit(1)

    def send_command(self, cmd):
        if self.engine.poll() is not None:
            return
        self.engine.stdin.write(cmd + '\n')
        self.engine.stdin.flush()

    def read_response(self):
        if self.engine.poll() is not None:
            return ""
        return self.engine.stdout.readline().strip()

    def select_mode_screen(self):
        running = True
        while running:
            self.screen.fill(WHITE)
            title = self.large_font.render("3D Connect 4", True, BLACK)
            opt1 = self.font.render("1. Human vs Human", True, BLACK)
            opt2 = self.font.render("2. Human vs AI (Red vs Yellow)", True, BLACK)
            opt3 = self.font.render("3. AI vs AI", True, BLACK)
            
            self.screen.blit(title, (250, 100))
            self.screen.blit(opt1, (250, 250))
            self.screen.blit(opt2, (250, 300))
            self.screen.blit(opt3, (250, 350))
            
            pygame.display.flip()
            
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit()
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_1:
                        self.mode = 0
                        running = False
                    elif event.key == pygame.K_2:
                        self.mode = 1
                        running = False
                    elif event.key == pygame.K_3:
                        self.mode = 2
                        running = False

    def update_board_state(self):
        self.send_command("print_raw")
        line = self.read_response()
        if line.startswith("board_state"):
            parts = line.split()
            self.board_a = int(parts[1])
            self.board_b = int(parts[2])

    def project(self, x, y, z):
        """ Projects 3D coordinates (x,y,z) to 2D screen coordinates. """
        # Center of the board (0..3) is at 1.5
        cx, cy, cz = 1.5, 1.5, 1.5
        
        # Translate to center
        tx, ty, tz = x - cx, y - cy, z - cz
        
        # 1. Rotate around Y axis (Horizontal)
        # x' = x cos θ - z sin θ
        # z' = x sin θ + z cos θ
        rx = tx * math.cos(self.angle_y) - tz * math.sin(self.angle_y)
        rz = tx * math.sin(self.angle_y) + tz * math.cos(self.angle_y)
        
        # 2. Rotate around X axis (Vertical Tilt)
        # y' = y cos φ - z' sin φ
        # z'' = y sin φ + z' cos φ
        ry = ty * math.cos(self.angle_x) - rz * math.sin(self.angle_x)
        rz_final = ty * math.sin(self.angle_x) + rz * math.cos(self.angle_x)
        
        # 3. Perspective Projection
        # Camera is at z = -camera_dist
        depth = rz_final + self.camera_dist
        if depth <= 0: depth = 0.1 # Prevent division by zero
        
        scale = self.fov / depth
        
        # Map to screen (Unit scale = 100 pixels)
        screen_x = WINDOW_SIZE[0] / 2 + rx * scale * 100
        screen_y = WINDOW_SIZE[1] / 2 - ry * scale * 100 # Invert Y for screen
        
        return screen_x, screen_y, scale, depth

    def draw_3d(self):
        self.screen.fill(BG_COLOR)
        
        render_list = []
        
        # --- 1. Base Plate ---
        for r in range(4):
            for c in range(4):
                # Draw a thick base block for each column
                y_top = -0.1
                y_bot = -0.3
                w = 0.4
                
                # Top Face
                corners_top = [
                    (c - w, y_top, r - w),
                    (c + w, y_top, r - w),
                    (c + w, y_top, r + w),
                    (c - w, y_top, r + w)
                ]
                proj_top = [self.project(*p) for p in corners_top]
                if all(p[3] > 0 for p in proj_top):
                    avg_z = sum(p[3] for p in proj_top) / 4
                    poly_pts = [(p[0], p[1]) for p in proj_top]
                    render_list.append({'type': 'poly', 'points': poly_pts, 'color': BROWN, 'z': avg_z})
                
                # Side Faces (Thickness)
                walls = [
                    [(c - w, r - w), (c + w, r - w)], # Back
                    [(c + w, r - w), (c + w, r + w)], # Right
                    [(c + w, r + w), (c - w, r + w)], # Front
                    [(c - w, r + w), (c - w, r - w)], # Left
                ]
                for p1_xz, p2_xz in walls:
                    wall_corners = [(p1_xz[0], y_top, p1_xz[1]), (p2_xz[0], y_top, p2_xz[1]), (p2_xz[0], y_bot, p2_xz[1]), (p1_xz[0], y_bot, p1_xz[1])]
                    proj_wall = [self.project(*p) for p in wall_corners]
                    if all(p[3] > 0 for p in proj_wall):
                        avg_z = sum(p[3] for p in proj_wall) / 4
                        poly_pts = [(p[0], p[1]) for p in proj_wall]
                        render_list.append({'type': 'poly', 'points': poly_pts, 'color': DARK_BROWN, 'z': avg_z})

        # --- 2. Vertical Pillars (Segmented) ---
        for r in range(4):
            for c in range(4):
                # Draw segments for each level to allow proper Z-sorting with pieces
                for d in range(4):
                    y_start = -0.2 if d == 0 else d - 0.5
                    y_end = d + 0.5
                    if d == 3: y_end = 3.5
                    
                    p1 = self.project(c, y_start, r)
                    p2 = self.project(c, y_end, r)
                    
                    if p1[3] > 0 and p2[3] > 0:
                        avg_z = (p1[3] + p2[3]) / 2
                        avg_z += 0.1 # Push back slightly so pieces draw on top
                        scale = (p1[2] + p2[2]) / 2
                        width = max(3, int(12 * scale))
                        render_list.append({'type': 'pillar', 'p1': (p1[0], p1[1]), 'p2': (p2[0], p2[1]), 'z': avg_z, 'width': width, 'color': (120, 120, 120)})

        # --- 3. Generate Pieces ---
        for d in range(4):
            for r in range(4):
                for c in range(4):
                    shift = 63 - (d * 16 + r * 4 + c)
                    mask = 1 << shift
                    color = None
                    if (self.board_a & mask): color = RED
                    elif (self.board_b & mask): color = YELLOW
                    
                    if color:
                        proj = self.project(c, d, r)
                        render_list.append({'type': 'circle', 'pos': proj, 'color': color, 'z': proj[3], 'r': 35 * proj[2]})

        # --- 4. Ghost Piece (Mouse Hover) ---
        mx, my = pygame.mouse.get_pos()
        best_dist = 60 # Pixel threshold for selection
        self.hover_rc = None
        
        # Check each column to see if mouse is close to it
        for r in range(4):
            for c in range(4):
                # Find the first empty level in this column
                d_target = 0
                while d_target < 4:
                    shift = 63 - (d_target * 16 + r * 4 + c)
                    if ((self.board_a | self.board_b) & (1 << shift)):
                        d_target += 1
                    else:
                        break
                
                if d_target < 4:
                    # Project where the piece would go
                    proj = self.project(c, d_target, r)
                    dx, dy = proj[0] - mx, proj[1] - my
                    dist = math.sqrt(dx*dx + dy*dy)
                    
                    if dist < best_dist:
                        best_dist = dist
                        self.hover_rc = (r, c)
                        ghost_color = (255, 100, 100, 150) if self.turn == 1 else (255, 255, 100, 150)
                        render_list.append({'type': 'ghost', 'pos': proj, 'color': ghost_color, 'z': proj[3], 'r': 35 * proj[2]})

        # --- 5. Sort and Draw ---
        # Painter's algorithm: Draw furthest items first
        render_list.sort(key=lambda x: x['z'], reverse=True)

        for item in render_list:
            if item['type'] == 'poly':
                pygame.draw.polygon(self.screen, item['color'], item['points'])
                pygame.draw.polygon(self.screen, BLACK, item['points'], 1)
            elif item['type'] == 'pillar':
                pygame.draw.line(self.screen, item['color'], item['p1'], item['p2'], item['width'])
                pygame.draw.line(self.screen, (180, 180, 180), item['p1'], item['p2'], max(1, int(item['width'] * 0.3)))
            elif item['type'] == 'line':
                pygame.draw.line(self.screen, item['color'], (item['p1'][0], item['p1'][1]), (item['p2'][0], item['p2'][1]), 2)
            elif item['type'] == 'circle':
                x, y = int(item['pos'][0]), int(item['pos'][1])
                r = int(item['r'])
                color = item['color']
                
                # Base sphere
                pygame.draw.circle(self.screen, color, (x, y), r)
                
                # Highlight (simulated light from top-left)
                light_color = (min(255, color[0] + 60), min(255, color[1] + 60), min(255, color[2] + 60))
                pygame.draw.circle(self.screen, light_color, (int(x - r*0.2), int(y - r*0.2)), int(r*0.6))
                
                # Specular highlight
                pygame.draw.circle(self.screen, (255, 255, 255), (int(x - r*0.35), int(y - r*0.35)), int(r*0.25))
                
                # Outline
                pygame.draw.circle(self.screen, BLACK, (x, y), r, 1)
            elif item['type'] == 'ghost':
                # Draw transparent circle
                s = pygame.Surface((int(item['r']*2)+2, int(item['r']*2)+2), pygame.SRCALPHA)
                pygame.draw.circle(s, item['color'], (int(item['r']), int(item['r'])), int(item['r']))
                self.screen.blit(s, (item['pos'][0]-item['r'], item['pos'][1]-item['r']))

        # --- 6. UI Overlay ---
        status_text = f"Turn: {'Player A (Red)' if self.turn == 1 else 'Player B (Yellow)'}"
        if self.game_over:
            status_text = f"Game Over! {self.winner} (Press R to Reset)"
        
        s_surf = self.font.render(status_text, True, WHITE)
        self.screen.blit(s_surf, (50, 20))
        
        help_text = self.font.render("Rotate: Arrows or Right-Click Drag | Reset: R", True, GRAY)
        self.screen.blit(help_text, (50, WINDOW_SIZE[1] - 50))

        pygame.display.flip()

    def make_move(self, r, c):
        self.send_command(f"move {r} {c} {self.turn}")
        resp = self.read_response()
        if resp.startswith("made_move"):
            parts = resp.split()
            status = parts[5]
            self.update_board_state()
            
            if status == "win_A":
                self.game_over = True
                self.winner = "Player A Wins!"
            elif status == "win_B":
                self.game_over = True
                self.winner = "Player B Wins!"
            elif status == "draw":
                self.game_over = True
                self.winner = "Draw!"
            else:
                self.turn = 3 - self.turn
            return True
        return False

    def run_ai(self):
        pygame.display.set_caption("3D Connect 4 - AI Thinking...")
        pygame.event.pump()
        self.send_command(f"search {self.ai_depth} {self.turn}")
        resp = self.read_response()
        if resp.startswith("bestmove"):
            parts = resp.split()
            r, c = int(parts[1]), int(parts[2])
            self.make_move(r, c)
        pygame.display.set_caption("3D Connect 4")

    def reset_game(self):
        self.send_command("new")
        self.board_a = 0
        self.board_b = 0
        self.turn = 1
        self.game_over = False
        self.winner = None

    def run(self):
        clock = pygame.time.Clock()
        running = True
        
        while running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.MOUSEBUTTONDOWN and not self.game_over:
                    if event.button == 1: # Left Click
                        if self.mode == 0 or (self.mode == 1 and self.turn == 1):
                            if self.hover_rc:
                                self.make_move(*self.hover_rc)
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_r:
                        self.reset_game()
            
            # Input Handling (Rotation)
            keys = pygame.key.get_pressed()
            if keys[pygame.K_LEFT]: self.angle_y += 0.05
            if keys[pygame.K_RIGHT]: self.angle_y -= 0.05
            if keys[pygame.K_UP]: self.angle_x += 0.05
            if keys[pygame.K_DOWN]: self.angle_x -= 0.05
            
            if pygame.mouse.get_pressed()[2]: # Right click drag
                rel = pygame.mouse.get_rel()
                self.angle_y += rel[0] * 0.01
                self.angle_x += rel[1] * 0.01
            else:
                pygame.mouse.get_rel() # Clear relative movement
            
            self.draw_3d()
            
            if not self.game_over:
                if self.mode == 2:
                    self.run_ai()
                elif self.mode == 1 and self.turn == 2:
                    self.run_ai()
            
            clock.tick(30)
        
        self.send_command("quit")
        self.engine.terminate()
        pygame.quit()

if __name__ == "__main__":
    game = Connect43D()
    game.run()