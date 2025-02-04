#pragma once

class SDL_Exception final : public std::runtime_error{
public:
   explicit SDL_Exception(const std::string &message) : std::runtime_error(message + "\n" +SDL_GetError()) {}
};