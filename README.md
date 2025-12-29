# Human-Centered Game UI/HUD Redesign

This is a top-down 2D zombie shooter game made in C++ using SFML. The game is the combined result of a 2D game development course, and later, Human-Centered Computing course.

This project is a redesign of a baseline game orginally created in collaboration with two other students. Building on that foundation, I first updated the game to a more playable version, and then redesigned the UI and HUD to preform usability testing and evaluation.

Original repo: [GitHub link](https://github.com/Huginho8/TDCod)

---

## Technologies

- C++
- SFML
- Visual Studio
- GitHub

---

## Features

**Gameplay Mechanics**
- Player movement, sprinting, aiming, shooting, reloading, weapon swapping.
- Enemy behavior with player tracking and lunge attacks.
- Wave-based round system that structures progression, pacing, and difficulty.

**Physics and Collisions**
- Custom 2D physics interactions between player, zombies, and bullets. 

**Visual Effects**
- Explosions and blood effects are implemented using a lightweight kinematic particle system.
- Supports persistent trace effects (e.g. blood stains and explosion residue).

**Animation System**
- Frame based animations with state transistions and sprite rotation.

**Menus**
- Main menu with transition into gameplay.
- In-game pause menu providing access to audio settings and an in-game control reference panel.

**UI & HUD Redesign**
- Redesigned UI and HUD to improve readability, information hierarchy, and player feedback.

---

## Running the Game

### Prerequisites
- SFML 2.6.x
- C++ compiler
- GitHub

### Steps

1. Clone the repository
2. Set up SFML
3. Build and run the project

---

## Video Showcase

*Single video showcasing the progression of the game across the three iterations.*
- **[Baseline Game](https://www.youtube.com/watch?v=VIDEO_ID&t=0s)**
- **[Foundation Overhaul](https://www.youtube.com/watch?v=VIDEO_ID&t=1m30s)**
- **[HCI Redesign](https://www.youtube.com/watch?v=VIDEO_ID&t=2m48s)**

### Screenshot
![UI Redesign Screenshot](docs\images\TDCod-HCI-Redesign-Thumbnail.png)

---

## HCI Redesign and Usability Evaluation

This redesign focused on improving the player interface, HUD, and feedback for better usability, readability, and immersion.

A usability study was conducted to evaluate improvements and was documented in a research paper:
[Research Paper PDF](docs/HCC_Final_Project.pdf)
