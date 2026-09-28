package auth

import (
	"fmt"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
)

// OperatorClaims are the custom fields embedded in every operator JWT.
type OperatorClaims struct {
	OperatorID uuid.UUID `json:"operator_id"`
	Username   string    `json:"username"`
	jwt.RegisteredClaims
}

// IssueOperatorJWT creates a signed HS256 JWT valid for 8 hours.
// The raw token string and its expiry time are returned so the caller
// can pass both to the client in the login response.
func IssueOperatorJWT(operatorID uuid.UUID, username, secret string) (string, time.Time, error) {
	expiry := time.Now().Add(8 * time.Hour)
	claims := OperatorClaims{
		OperatorID: operatorID,
		Username:   username,
		RegisteredClaims: jwt.RegisteredClaims{
			Subject:   operatorID.String(),
			ExpiresAt: jwt.NewNumericDate(expiry),
			IssuedAt:  jwt.NewNumericDate(time.Now()),
		},
	}
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	signed, err := token.SignedString([]byte(secret))
	if err != nil {
		return "", time.Time{}, fmt.Errorf("issue operator jwt: %w", err)
	}
	return signed, expiry, nil
}

// ValidateOperatorJWT parses and validates a JWT, returning the embedded
// claims on success.  Returns an error for expired, tampered, or
// malformed tokens.
func ValidateOperatorJWT(tokenStr, secret string) (*OperatorClaims, error) {
	token, err := jwt.ParseWithClaims(tokenStr, &OperatorClaims{}, func(t *jwt.Token) (interface{}, error) {
		if _, ok := t.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", t.Header["alg"])
		}
		return []byte(secret), nil
	})
	if err != nil {
		return nil, fmt.Errorf("validate operator jwt: %w", err)
	}
	claims, ok := token.Claims.(*OperatorClaims)
	if !ok || !token.Valid {
		return nil, fmt.Errorf("validate operator jwt: invalid claims")
	}
	return claims, nil
}
