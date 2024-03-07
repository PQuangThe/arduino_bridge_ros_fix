
int64_t readEncoder(int i) 
{
  if (i == LEFT) return encoder_left.getCount();
  else return encoder_right.getCount();
}

/* Wrap the encoder reset function */
void resetEncoder(int i) 
{
  if (i == LEFT){
    encoder_left.clearCount();
    return;
  } else { 
    encoder_right.clearCount();
    return;
  }
}

/* Wrap the encoder reset function */
void resetEncoders() 
{
  resetEncoder(LEFT);
  resetEncoder(RIGHT);
}


