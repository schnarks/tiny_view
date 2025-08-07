#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h> // Include for stat function
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define INITIAL_CAPACITY 10

#define DEFAULT_FONT_SIZE 18
#define DEFAULT_BAR_HEIGHT 30
typedef struct {
    char** paths;      // Array of image paths
    int size;         // Current number of images
    int capacity;     // Current capacity of the array
} ImageList;

void initImageList(ImageList* list) {
    list->paths = (char**)malloc(INITIAL_CAPACITY * sizeof(char*)); // Cast to char**
    list->size = 0;
    list->capacity = INITIAL_CAPACITY;
}

void addImagePath(ImageList* list, const char* path) {
    // Resize if necessary
    if (list->size >= list->capacity) {
        list->capacity *= 2;
        list->paths = (char**)realloc(list->paths, list->capacity * sizeof(char*)); // Cast to char**
    }
    // Allocate memory for the new path and copy it
    list->paths[list->size] = (char*)malloc(strlen(path) + 1); // Cast to char*
    strcpy(list->paths[list->size], path);
    list->size++;
}

void freeImageList(ImageList* list) {
    for (int i = 0; i < list->size; i++) {
        free(list->paths[i]); // Free each path
    }
    free(list->paths); // Free the array
}

bool isImageFile(const char* filename) {
    const char* extensions[] = { ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tif", ".tiff", ".webp", ".ico" };
    for (int i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        if (strstr(filename, extensions[i]) != NULL) {
            return true; // Found a valid image extension
        }
    }
    return false; // Not an image file
}

void scanDirectory(const char* dirPath, ImageList* list) { // Pass ImageList as a parameter
    struct dirent* entry;
    DIR* dir = opendir(dirPath);
    if (dir == NULL) {
        printf("Could not open directory: %s\n", dirPath);
        return;
    }
    printf("Images in directory '%s':\n", dirPath);
    while ((entry = readdir(dir)) != NULL) {
        if (isImageFile(entry->d_name)) {
            char fullPath[1024];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, entry->d_name); // Create full path
            addImagePath(list, fullPath); // Add the full path to the image list
            printf(" - %s\n", fullPath);
        }
    }
    closedir(dir);
}

char* getImagePath(int index, ImageList* imageList) {
    if (index < 0 || index >= imageList->size) {
        printf("Error: Index %d is out of bounds for the image list.\n", index);
        return NULL; // Return NULL if the index is invalid
    }
    return imageList->paths[index]; // Return the image path at the specified index
}

void loadImage(char* path, SDL_Texture** imageTexture, SDL_Renderer* renderer, int* width, int* height, float* aspectRatio) {
    // Load the new surface
    SDL_Surface* newSurface = IMG_Load(path);
    if (!newSurface) {
        printf("Failed to load image: %s\n", IMG_GetError());
        return; // Exit if the image fails to load
    }
    // Destroy the old texture if it exists
    if (*imageTexture) {
        SDL_DestroyTexture(*imageTexture);
    }
    // Create a new texture from the new surface
    *imageTexture = SDL_CreateTextureFromSurface(renderer, newSurface);
    // Update the width and height with the new surface dimensions
    *width = newSurface->w;
    *height = newSurface->h;
    SDL_FreeSurface(newSurface); // Free the new surface after creating the texture
    *aspectRatio = (float)*width / (float)*height;
}

void autoCenterZoom(int imgWidth, int imgHeight, int frameWidth, int frameHeight, float* zoomFactor, int* bgX, int* bgY) {
    // Calculate the zoom factor based on the frame size and image size
    float widthRatio = (float)frameWidth / imgWidth;
    float heightRatio = (float)frameHeight / imgHeight;

    // Use the smaller ratio to ensure the image fits within the frame
    *zoomFactor = (widthRatio < heightRatio) ? widthRatio : heightRatio;

    // Calculate the new dimensions of the image after applying the zoom
    int newWidth = (int)(imgWidth * (*zoomFactor));
    int newHeight = (int)(imgHeight * (*zoomFactor));

    // Center the image in the frame
    *bgX = (frameWidth - newWidth) / 2;
    *bgY = (frameHeight - newHeight) / 2;
}

void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y) {
    SDL_Color textColor = {255, 255, 255}; // White color for the text
    char* newText = malloc(strlen(text) + 2); // +2 for ':' and '\0'
    if (newText == NULL) {
        // Handle memory allocation failure
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    // Concatenate ':' in front of the original text
    snprintf(newText, strlen(text) + 2, ":%s", text);

    SDL_Surface* textSurface = TTF_RenderText_Solid(font, newText, textColor);
    if (!textSurface) {
        printf("Failed to create text surface! SDL_ttf Error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);

    // Get the width and height of the texture
    int textWidth = 0, textHeight = 0;
    SDL_QueryTexture(textTexture, NULL, NULL, &textWidth, &textHeight);

    SDL_Rect dstRect = {x, y, textWidth, textHeight};
    SDL_RenderCopy(renderer, textTexture, NULL, &dstRect);
    SDL_DestroyTexture(textTexture); // Clean up the texture
}

void renderCommandBar(SDL_Renderer* renderer, SDL_Rect* bar, TTF_Font* font, const char* cmd, int frameWidth, int frameHeight) {

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, bar);

    // Render the command text
    renderText(renderer, font, cmd, bar->x + 5, bar->y + 5); // Add some padding
    SDL_RenderPresent(renderer);
}

void non_blocking_system(const char *command) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Error forking");
        return; // Handle error
    }

    if (pid > 0) {
        // Parent process, return immediately
        return; // Command is running in the background
    }

    // Child process
    if (setsid() < 0) {
        perror("Error creating new session");
        exit(EXIT_FAILURE);
    }

    // Redirect standard output and error to /dev/null
    int devNull = open("/dev/null", O_RDWR);
    if (devNull >= 0) {
        dup2(devNull, STDOUT_FILENO);
        dup2(devNull, STDERR_FILENO);
        close(devNull);
    }

    // Execute the command using execl
    execl("/bin/sh", "sh", "-c", command, (char *)NULL);

    // If execl fails
    perror("Error executing command");
    exit(EXIT_FAILURE);
}

void bottomCommandBar(SDL_Window* window, SDL_Renderer* renderer, TTF_Font* font, int frameWidth, int frameHeight, char* activeFile) {
    // Show Bottom bar
    SDL_Rect bar = {0,frameHeight-DEFAULT_BAR_HEIGHT, frameWidth, DEFAULT_BAR_HEIGHT};
    const int maxCmdLength = 256;
    char cmd[maxCmdLength]; // Command buffer:
    // Initialize the array to zero
    for (int i = 0; i < maxCmdLength; i++) {
        cmd[i] = 0;
    }    int cmdLength = 0; // Current length of the command
    SDL_Event e;
    // Initial render
    renderCommandBar(renderer, &bar, font, "", frameWidth, frameHeight);

    while (true) {
        SDL_WaitEvent(&e);
        if (e.type == SDL_QUIT) {
            break;
        }
        // Keydown
        else if (e.type == SDL_KEYDOWN) {
            if (((e.key.keysym.sym == SDLK_q) && (SDL_GetModState() & KMOD_CTRL)) || ((e.key.keysym.sym == SDLK_c) && (SDL_GetModState() & KMOD_CTRL))|| e.key.keysym.sym == SDLK_ESCAPE) {
                printf("CMD Bar Escape Triggered\n");
                break;
            }
            // Check for Enter to end the command
            if (e.key.keysym.sym == SDLK_RETURN) {
                size_t commandLength = strlen(cmd) + strlen(activeFile) + 2;

                if (strcmp(cmd, "q") == 0) {
                    exit(0);
                }

                char* fullCommand = (char*)malloc(commandLength * sizeof(char));

                if (fullCommand == NULL) {
                    perror("Failed to allocate memory");
                    return;
                }

                // Construct the full command
                snprintf(fullCommand, commandLength, "%s %s", cmd, activeFile);

                // Execute the command
                non_blocking_system(fullCommand);
                // Free the allocated memory
                free(fullCommand);

                // Reset command buffer
                memset(cmd, 0, sizeof(cmd));
                cmdLength = 0;
                break;
            }
            // Handle backspace
            else if (e.key.keysym.sym == SDLK_BACKSPACE) {
                if (cmdLength > 0) {
                    cmdLength--;
                    cmd[cmdLength] = '\0'; // Null-terminate the string
                }
            }
            // Add character to cmd buffer
            else if (cmdLength < maxCmdLength - 1) { // Leave space for null terminator
                cmd[cmdLength] = e.key.keysym.sym; // Store the character
                cmdLength++;
                cmd[cmdLength] = '\0'; // Null-terminate the string
            }
            // Render the updated command bar
            renderCommandBar(renderer, &bar, font, cmd, frameWidth, frameHeight);
        }
    }
}

int main(int argc, char* argv[]) {

    bool enableAutoZoomCentering = true;
    int width = 800;
    int height = 600;

    // Declare variables for image dimensions
    int imgWidth = 0;
    int imgHeight = 0;
    float aspectRatio = 0;

    // Zoom factor and position variables
    float zoomFactor = 1.0f; // Initial zoom level
    const float zoomStep = 0.1f; // Amount to zoom in/out
    int bgX = 0; // Initial X position of the background
    int bgY = 0; // Initial Y position of the background

    SDL_Texture* imageTexture = NULL; // Declare imageTexture
    SDL_Texture* texture = NULL; // Declare texture

    struct stat pathStat;

    ImageList imageList; // Declare the ImageList variable
    initImageList(&imageList); // Initialize the image list
    int currentIndex = 0; // Declare and initialize currentIndex

    // Determine the directory and image file
    char dirPath[1024] = ".";
    char* imagePath = NULL;

    char* fontPath =  "/usr/share/fonts/gnu-free/FreeSans.otf";


    if (argc > 1) {
        if (argc == 2)
        {
            imagePath = argv[1];
        }
        else if (argc > 2)
        {
            imagePath = argv[2];
            printf("%s", imagePath);
            fontPath = argv[1];
        }
        stat(imagePath, &pathStat);
        if (S_ISDIR(pathStat.st_mode)) {
            strcpy(dirPath, imagePath);
        } else {
            char* lastSlash = strrchr(imagePath, '/');
            if (lastSlash) {
                strncpy(dirPath, imagePath, lastSlash - imagePath);
                dirPath[lastSlash - imagePath] = '\0';
            } else {
                strcpy(dirPath, ".");
            }
        }
    }
    // Scan the directory for images
    scanDirectory(dirPath, &imageList);

    // If an image file is provided, check if it's valid and set the current index
    if (imagePath && !S_ISDIR(pathStat.st_mode)) {
        if (isImageFile(imagePath)) {
            addImagePath(&imageList, imagePath); // Add the provided image to the list
            currentIndex = imageList.size - 1; // Set current index to the newly added image
        } else {
            printf("Provided file is not a valid image: %s\n", imagePath);
            // If the directory contains images, set currentIndex to 0
            if (imageList.size > 0) {
                currentIndex = 0; // Use the first image in the list
            } else {
                return 1; // Exit if no images are found
            }
        }
    } else {
        // If no image file is provided, just use the first image in the list
        if (imageList.size > 0) {
            currentIndex = 0; // Use the first image in the list
        } else {
            printf("No images found in the directory.\n");
            return 1; // Exit if no images are found
        }
    }

    // Load the initial image
    char* currentImagePath = getImagePath(currentIndex, &imageList);


    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    // Initialize SDL_image
    if (IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_TIF | IMG_INIT_WEBP) == 0) {
        printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        printf("SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        return 1;
    }

    TTF_Font* font = TTF_OpenFont(fontPath, DEFAULT_FONT_SIZE);
    if (!font) {
        printf("Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("TinyView", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    loadImage(currentImagePath, &imageTexture, renderer, &imgWidth, &imgHeight, &aspectRatio); // Load the new image

    // Main loop
    SDL_Event e;
    int quit = 0;
    Uint32 startTime;
    while (!quit) {
        SDL_WaitEvent(&e);
        SDL_GetWindowSize(window, &width, &height);
        if (e.type == SDL_QUIT) {
            quit = 1;
        }
        // Keydown
        else if (e.type == SDL_KEYDOWN) {
            // Check for Ctrl + Q to quit
            if (((e.key.keysym.sym == SDLK_q) && (SDL_GetModState() & KMOD_CTRL)) || ((e.key.keysym.sym == SDLK_c) && (SDL_GetModState() & KMOD_CTRL))) {
                quit = 1;
            }
            // Zoom in
            else if (e.key.keysym.sym == SDLK_UP || e.key.keysym.sym == SDLK_k) {
                zoomFactor += zoomStep;
            }
            // Zoom out
            else if (e.key.keysym.sym == SDLK_DOWN || e.key.keysym.sym == SDLK_j) {
                zoomFactor = (zoomFactor > zoomStep) ? (zoomFactor - zoomStep) : zoomStep; // Prevent zooming out too much
            }
            // Move left
            else if (e.key.keysym.sym == SDLK_d) {
                bgX -= 10; // Move left by 10 pixels
            }
            // Move right
            else if (e.key.keysym.sym == SDLK_a) {
                bgX += 10; // Move right by 10 pixels
            }
            // Move up
            else if (e.key.keysym.sym == SDLK_s) {
                bgY -= 10; // Move up
            }
            // Move down
            else if (e.key.keysym.sym == SDLK_w) {
                bgY += 10; // Move down by 10 pixels
            }
            // Next Image
            else if (e.key.keysym.sym == SDLK_RIGHT || e.key.keysym.sym == SDLK_l) {

                currentIndex = (currentIndex + 1) % imageList.size; // Loop back to the first image
                char* currentImagePath = getImagePath(currentIndex, &imageList);
                printf("%s\n", currentImagePath);
                loadImage(currentImagePath, &imageTexture, renderer, &imgWidth, &imgHeight, &aspectRatio); //Load the new image
                enableAutoZoomCentering = true;
            }

            // Previous Image
            else if (e.key.keysym.sym == SDLK_LEFT || e.key.keysym.sym == SDLK_h) {
                currentIndex = (currentIndex - 1 + imageList.size) % imageList.size; // Loop back to the last image
                char* currentImagePath = getImagePath(currentIndex, &imageList);
                printf("%s\n", currentImagePath);
                loadImage(currentImagePath, &imageTexture, renderer, &imgWidth, &imgHeight, &aspectRatio); // Load the new image
                enableAutoZoomCentering = true;
            }
            else if (e.key.keysym.sym == SDLK_COLON) {
                bottomCommandBar(window, renderer, font, width, height, getImagePath(currentIndex,&imageList));
            }
        }
        else if (e.type == SDL_TEXTINPUT) {
            if (strcmp(e.text.text, ":") == 0) {
                bottomCommandBar(window, renderer, font, width, height, getImagePath(currentIndex,&imageList));
            }
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Set to black
        SDL_RenderClear(renderer);

        if (enableAutoZoomCentering){
            autoCenterZoom(imgWidth, imgHeight, width, height, &zoomFactor, &bgX, &bgY);
            enableAutoZoomCentering = false;
        }
        // Calculate the destination rectangle for the background texture
        int bgWidth = (int)(imgWidth * zoomFactor); // Use width for scaling
        int bgHeight = (int)(bgWidth / aspectRatio); // Maintain aspect ratio based on bgWidth
        SDL_Rect dstRect = { bgX, bgY, bgWidth, bgHeight };

        // Render the background texture with zoom and position
        SDL_RenderCopy(renderer, imageTexture, NULL, &dstRect);
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    SDL_DestroyTexture(imageTexture); // Destroy the background texture
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    IMG_Quit(); // Quit SDL_image

    return 0;
}
